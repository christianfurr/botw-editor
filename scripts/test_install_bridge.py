"""Installer guard and patch-idempotence tests; never opens or updates the game."""
import importlib.util
import base64
import hashlib
import json
import os
import plistlib
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

sys.dont_write_bytecode = True
SPEC = importlib.util.spec_from_file_location("botw_bridge_installer", Path(__file__).with_name("install-bridge.py"))
installer = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = installer
SPEC.loader.exec_module(installer)


class PatchTests(unittest.TestCase):
    def test_patch_is_detected_before_and_after_application(self):
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            subprocess.run(["git", "init", "-q", str(folder)], check=True)
            (folder / "sample.c").write_text("before\n")
            patch = folder / "bridge.patch"
            patch.write_text("diff --git a/sample.c b/sample.c\n--- a/sample.c\n+++ b/sample.c\n@@ -1 +1 @@\n-before\n+after\n")
            self.assertEqual(installer.patch_state(folder, patch), "needs-install")
            installer.checked(["git", "apply", str(patch)], cwd=folder)
            self.assertEqual(installer.patch_state(folder, patch), "integrated")
            self.assertEqual((folder / "sample.c").read_text(), "after\n")
            (folder / "sample.c").write_text("unrelated conflicting edit\n")
            with self.assertRaises(installer.InstallError):
                installer.patch_state(folder, patch)

    def test_clone_does_not_overwrite_and_preserves_symlinks(self):
        if sys.platform != "darwin":
            self.skipTest("APFS clonefile helper requires macOS")
        with tempfile.TemporaryDirectory() as directory:
            folder = Path(directory)
            source, destination = folder / "source", folder / "backup"
            source.write_bytes(b"original profile fixture")
            installer.clone_copy(source, destination)
            self.assertEqual(destination.read_bytes(), source.read_bytes())
            with self.assertRaises(installer.InstallError):
                installer.clone_copy(source, destination)
            link = folder / "link"
            link.symlink_to("source")
            installer.clone_copy(link, folder / "link-backup")
            self.assertEqual(os.readlink(folder / "link-backup"), "source")


class PreflightTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.folder = Path(self.temporary.name)
        self.repo, self.app, self.work = self.folder / "repo", self.folder / "BOTW.app", self.folder / "work"
        self.repo.mkdir()
        (self.repo / "update_runtime.py").write_text("# test fixture; never executed\n")
        for relative in installer.SOURCE_FILES:
            path = self.repo / relative
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text("upstream test fixture\n")
        for name in installer.HEADERS:
            (self.repo / "games/totk/runtime" / name).write_bytes((installer.ROOT / "runtime" / name).read_bytes())
        (self.work / "totk").mkdir(parents=True)
        resources = self.app / "Contents/Resources/totk"
        (resources / "rewritten/botw160").mkdir(parents=True)
        (resources / "build").mkdir()
        (resources / "build/totk-native").write_bytes(b"test-runtime")
        self.info = self.app / "Contents/Info.plist"
        self.info.write_bytes(plistlib.dumps({"CFBundleIdentifier": "org.macnativegames.botw", "CFBundleShortVersionString": "1.6.0"}))
        self.args = installer.parse_args(["--repo", str(self.repo), "--app", str(self.app), "--work", str(self.work), "--check"])
        self.patches = [
            mock.patch.object(installer.platform, "system", return_value="Darwin"),
            mock.patch.object(installer.platform, "machine", return_value="arm64"),
            mock.patch.object(installer.platform, "mac_ver", return_value=("26.0", (), "")),
            mock.patch.object(installer, "checked", return_value=installer.PINNED_COMMIT),
            mock.patch.object(installer, "patch_state", return_value="integrated"),
            mock.patch.object(installer, "python314", return_value=Path(sys.executable)),
            mock.patch.object(installer, "active_game_processes", return_value=[]),
            mock.patch.object(installer, "APP_DATA", self.folder / "installer-state"),
            mock.patch.object(installer, "GAME_DATA", self.folder / "game-data"),
        ]
        for patch in self.patches:
            patch.start()

    def tearDown(self):
        for patch in reversed(self.patches):
            patch.stop()
        self.temporary.cleanup()

    def snapshot(self):
        return {str(path.relative_to(self.folder)): path.read_bytes()
                for path in self.folder.rglob("*") if path.is_file()}

    def test_integrated_preflight_is_read_only(self):
        before = self.snapshot()
        plan = installer.preflight(self.args)
        self.assertEqual(plan.integration, "integrated")
        self.assertEqual(plan.changes, [])
        self.assertFalse(plan.runtime_confirmed)
        self.assertEqual(self.snapshot(), before)
        self.assertFalse(installer.APP_DATA.exists())

    def test_unowned_changed_header_is_refused(self):
        (self.repo / "games/totk/runtime" / installer.HEADERS[0]).write_text("someone else's bridge\n")
        with self.assertRaisesRegex(installer.InstallError, "unowned"):
            installer.preflight(self.args)

    def test_recorded_owned_header_can_be_updated(self):
        plan = installer.preflight(self.args)
        relative = "games/totk/runtime/" + installer.HEADERS[0]
        path = self.repo / relative
        path.write_text("old installer-owned header\n")
        installer.write_json(plan.manifest_path, {"schema": 1, "repo": str(plan.repo), "app": str(plan.app),
            "owned_headers": {relative: installer.digest(path)}})
        self.assertIn(relative, installer.preflight(self.args).changes)

    def test_wrong_game_version_is_refused(self):
        self.info.write_bytes(plistlib.dumps({"CFBundleIdentifier": "org.macnativegames.botw", "CFBundleShortVersionString": "1.9.0"}))
        with self.assertRaisesRegex(installer.InstallError, "1.6.0"):
            installer.preflight(self.args)

    def test_active_game_is_refused_without_closing_it(self):
        with mock.patch.object(installer, "active_game_processes", return_value=[1234]):
            with self.assertRaisesRegex(installer.InstallError, "Quit it yourself"):
                installer.preflight(self.args)

    def test_changed_sources_after_backup_are_refused(self):
        plan = installer.preflight(self.args)
        (self.repo / installer.SOURCE_FILES[0]).write_text("edit after validation\n")
        with self.assertRaisesRegex(installer.InstallError, "Source changed"):
            installer.verify_unchanged(plan)

    def test_private_cache_hashes_are_checked(self):
        # A generated single transparent pixel; no game artwork or files.
        pixel = base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII=")
        checksum = hashlib.sha256(pixel).hexdigest()
        map_folder = installer.APP_DATA / "cache/map"
        map_folder.mkdir(parents=True)
        (map_folder / "map.png").write_bytes(pixel)
        (map_folder / "map.json").write_text(json.dumps({"schema": 1, "image": "map.png",
            "image_sha256": checksum, "width": 1, "height": 1, "orientation": "north-up",
            "world_bounds": {"min_x": -1, "max_x": 1, "min_z": -1, "max_z": 1}}))
        catalog = {item["id"] for item in json.loads((installer.ROOT / "Sources/catalog.json").read_text())}
        actor = sorted(catalog)[0]
        item_folder = installer.APP_DATA / "cache/items"
        item_folder.mkdir(parents=True)
        (item_folder / (actor + ".png")).write_bytes(pixel)
        (item_folder / "manifest.json").write_text(json.dumps({"schema": 1, "stock_count": 1,
            "icon_count": 1, "icons": {actor: {"file": actor + ".png", "sha256": checksum, "width": 1, "height": 1}},
            "catalog": {"total": len(catalog), "resolved": 1, "missing": sorted(catalog - {actor})}}))
        self.assertTrue(installer.map_cache_valid())
        self.assertTrue(installer.items_cache_valid())
        (map_folder / "map.png").write_bytes(pixel + b"corrupt")
        (item_folder / (actor + ".png")).unlink()
        self.assertFalse(installer.map_cache_valid())
        self.assertFalse(installer.items_cache_valid())

    def confirmed_plan(self):
        plan = installer.preflight(self.args)
        plan.runtime_confirmed = True
        installer.GAME_DATA.mkdir()
        (installer.GAME_DATA / "editor-destinations.tsv").write_text("test destination fixture\n")
        return plan

    def test_confirmed_runtime_reuses_assets_without_writes_or_rebuild(self):
        plan = self.confirmed_plan()
        before = self.snapshot()
        with mock.patch.object(installer, "map_cache_valid", return_value=True), \
             mock.patch.object(installer, "items_cache_valid", return_value=True), \
             mock.patch.object(installer, "stream_command") as stream, \
             mock.patch.object(installer, "clone_copy") as clone, \
             mock.patch.object(installer, "say"):
            installer.install(plan)
        stream.assert_not_called()
        clone.assert_not_called()
        self.assertEqual(self.snapshot(), before)
        self.assertFalse(installer.APP_DATA.exists())

    def test_confirmed_runtime_extracts_missing_assets_without_rebuild(self):
        plan = self.confirmed_plan()
        with mock.patch.object(installer, "map_cache_valid", side_effect=[False, True]), \
             mock.patch.object(installer, "items_cache_valid", side_effect=[False, True]), \
             mock.patch.object(installer, "stream_command") as stream, \
             mock.patch.object(installer, "clone_copy") as clone, \
             mock.patch.object(installer, "say"):
            installer.install(plan)
        clone.assert_not_called()
        self.assertEqual(stream.call_count, 2)
        for call, kind in zip(stream.call_args_list, ("map", "items")):
            command, log = call.args
            self.assertEqual(command[:4], [str(plan.python), str(installer.ROOT / f"scripts/extract_{kind}.py"), "--app", str(plan.app)])
            self.assertIn(str(installer.APP_DATA / "cache" / kind), command)
            self.assertTrue(log.is_relative_to(installer.APP_DATA / "logs"))
        self.assertFalse((installer.APP_DATA / "Backups").exists())

    def test_extraction_requires_verified_output_before_success(self):
        plan = self.confirmed_plan()
        with mock.patch.object(installer, "map_cache_valid", return_value=False), \
             mock.patch.object(installer, "stream_command"), \
             mock.patch.object(installer, "say"):
            with self.assertRaisesRegex(installer.InstallError, "hash-verified cache"):
                installer.install(plan)


if __name__ == "__main__":
    unittest.main(verbosity=2)
