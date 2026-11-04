#!/usr/bin/env python3
"""Install BOTW Editor's bridge into a supported local MacNativeGames build.

--check validates without changing files or rebuilding. Game files stay local.
"""
from __future__ import annotations

import argparse
import ctypes
import datetime as dt
import errno
import hashlib
import json
import math
import os
import platform
import plistlib
import re
import shutil
import stat
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


PINNED_COMMIT = "b1a49a26650eb31a986e7841f449bb44446c5208"
ROOT = Path(__file__).resolve().parents[1]
HEADERS = (
    "botw_editor.h", "botw_editor_inventory.h", "botw_editor_world.h",
    "botw_editor_teleport.h", "botw_editor_destinations.h",
)
SOURCE_FILES = (
    "games/totk/runtime/input.c", "games/totk/runtime/game_patches_botw.c",
)
APP_DATA = Path.home() / "Library/Application Support/BOTWEditor"
GAME_DATA = Path.home() / "Library/Application Support/MacNativeGames/BotW"
PATCH = ROOT / "runtime/macnativegames-botw160.patch"


class InstallError(Exception):
    pass


def say(message: str) -> None:
    print(message, flush=True)


def digest(path: Path) -> str:
    checksum = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            checksum.update(chunk)
    return checksum.hexdigest()


def run(command: list[str], *, cwd: Path | None = None) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, capture_output=True, text=True)


def checked(command: list[str], *, cwd: Path | None = None) -> str:
    result = run(command, cwd=cwd)
    if result.returncode:
        detail = (result.stderr or result.stdout).strip()
        raise InstallError(f"{Path(command[0]).name} failed: {detail}")
    return result.stdout.strip()


def read_json(path: Path) -> dict:
    if not path.exists():
        return {}
    try:
        value = json.loads(path.read_text())
    except (OSError, ValueError) as error:
        raise InstallError(f"Cannot read installer ownership record {path}: {error}") from error
    if not isinstance(value, dict) or value.get("schema") != 1:
        raise InstallError(f"Unsupported installer ownership record: {path}")
    return value


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    temporary = path.with_name(path.name + f".{os.getpid()}.tmp")
    with temporary.open("x", encoding="utf-8") as stream:
        os.chmod(temporary, 0o600)
        json.dump(value, stream, indent=2)
        stream.write("\n")
    temporary.replace(path)


def python314() -> Path:
    candidates = [shutil.which("python3.14"), "/opt/homebrew/bin/python3.14"]
    for candidate in dict.fromkeys(path for path in candidates if path):
        path = Path(candidate)
        if path.is_file():
            version = run([str(path), "-c", "import sys; print('%d.%d' % sys.version_info[:2])"])
            if not version.returncode and version.stdout.strip() == "3.14":
                return path
    raise InstallError("Python 3.14 is required by MacNativeGames. Install it with: brew install python@3.14")


def active_game_processes(app: Path) -> list[int]:
    executable = app / "Contents/Resources/totk/build/totk-native"
    result = run(["/bin/ps", "-axo", "pid=,comm="])
    if result.returncode:
        raise InstallError("Cannot check running game processes; quit BOTW and retry.")
    processes = []
    for line in result.stdout.splitlines():
        columns = line.strip().split(None, 1)
        if len(columns) == 2 and columns[1] == str(executable):
            try:
                processes.append(int(columns[0]))
            except ValueError:
                pass
    return processes


def patch_state(repo: Path, patch: Path = PATCH) -> str:
    forward = run(["git", "apply", "--check", str(patch)], cwd=repo)
    if not forward.returncode:
        return "needs-install"
    reverse = run(["git", "apply", "--reverse", "--check", str(patch)], cwd=repo)
    if not reverse.returncode:
        return "integrated"
    raise InstallError(
        "The runtime integration conflicts with local edits in input.c or game_patches_botw.c. "
        "No files were changed. Preserve your edits and use a compatible checkout.\n"
        + (forward.stderr or reverse.stderr).strip()
    )


def target_file(repo: Path, relative: str) -> Path:
    target = repo / relative
    if target.is_symlink() or not target.resolve().is_relative_to(repo):
        raise InstallError(f"Refusing a symlink or source path outside the checkout: {relative}")
    return target


def validate_patch_contents() -> None:
    try:
        text = PATCH.read_text()
    except OSError as error:
        raise InstallError(f"Missing packaged integration patch: {PATCH}") from error
    paths, sections = [], []
    for line in text.splitlines():
        if line.startswith("diff --git "):
            candidates = [relative for relative in SOURCE_FILES
                          if line == f"diff --git a/{relative} b/{relative}"]
            if len(candidates) != 1:
                raise InstallError("The packaged patch contains an unexpected source path.")
            sections.extend(candidates)
        if line.startswith("+++ b/"):
            paths.append(line[6:])
    if sorted(paths) != sorted(SOURCE_FILES) or sorted(sections) != sorted(SOURCE_FILES) or "GIT binary patch" in text:
        raise InstallError("The packaged patch does not contain exactly the expected two runtime source files.")


@dataclass
class Plan:
    repo: Path
    app: Path
    work: Path
    python: Path
    manifest_path: Path
    manifest: dict
    integration: str
    changes: list[str]
    source_hashes: dict[str, str | None]
    package_hashes: dict[str, str]
    runtime_confirmed: bool


def preflight(args: argparse.Namespace) -> Plan:
    if platform.system() != "Darwin" or platform.machine() != "arm64":
        raise InstallError("The bridge requires an Apple silicon Mac.")
    version = platform.mac_ver()[0]
    if not version or int(version.split(".")[0]) < 26:
        raise InstallError("The bridge requires macOS 26 or later.")
    repo, app, work = (path.expanduser().resolve() for path in (args.repo, args.app, args.work))
    if not repo.is_dir() or not (repo / "update_runtime.py").is_file():
        raise InstallError(f"Not a MacNativeGames checkout: {repo}")
    commit = checked(["git", "rev-parse", "HEAD"], cwd=repo)
    if commit != PINNED_COMMIT:
        raise InstallError(f"Unsupported MacNativeGames revision {commit}. This release requires {PINNED_COMMIT}.")
    if not (work / "totk").is_dir():
        raise InstallError(f"Not the original BOTW build work folder (expected its totk/ subfolder): {work}")
    try:
        info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    except (OSError, ValueError) as error:
        raise InstallError(f"Cannot read the target game app: {app}") from error
    versions = app / "Contents/Resources/totk/rewritten"
    modules = sorted(item.name for item in versions.iterdir() if item.is_dir()) if versions.is_dir() else []
    if info.get("CFBundleIdentifier") != "org.macnativegames.botw" or \
       info.get("CFBundleShortVersionString") != "1.6.0" or modules != ["botw160"]:
        raise InstallError("The target must be a MacNativeGames Breath of the Wild 1.6.0 app. Other versions are not supported.")
    runtime = app / "Contents/Resources/totk/build/totk-native"
    if not runtime.is_file():
        raise InstallError(f"The target app has no game runtime: {runtime}")
    processes = active_game_processes(app)
    if processes:
        raise InstallError(f"BOTW is running (PID {', '.join(map(str, processes))}). Quit it yourself, then retry; the installer will not close it.")
    py = python314()
    validate_patch_contents()
    integration = patch_state(repo)
    key = hashlib.sha256((str(repo) + "\0" + str(app)).encode()).hexdigest()[:24]
    manifest_path = APP_DATA / "Installations" / (key + ".json")
    manifest = read_json(manifest_path)
    if manifest and (manifest.get("repo") != str(repo) or manifest.get("app") != str(app)):
        raise InstallError("The installer ownership record belongs to a different checkout or app.")
    owned = manifest.get("owned_headers", {})
    source_hashes: dict[str, str | None] = {}
    package_hashes = {"patch": digest(PATCH)}
    changes = list(SOURCE_FILES) if integration == "needs-install" else []
    for relative in SOURCE_FILES:
        target = target_file(repo, relative)
        if not target.is_file():
            raise InstallError(f"Missing upstream runtime source: {relative}")
        source_hashes[relative] = digest(target)
    for name in HEADERS:
        packaged = ROOT / "runtime" / name
        if not packaged.is_file():
            raise InstallError(f"Missing packaged bridge header: {name}")
        package_hashes[name] = digest(packaged)
        relative = "games/totk/runtime/" + name
        target = target_file(repo, relative)
        current = digest(target) if target.is_file() else None
        if target.exists() and not target.is_file():
            raise InstallError(f"The bridge target is not a regular file: {relative}")
        source_hashes[relative] = current
        if current != package_hashes[name]:
            if current is not None and current != owned.get(relative):
                raise InstallError(f"Refusing to overwrite an unowned or locally edited bridge header: {relative}. Preserve it and retry from a compatible checkout.")
            changes.append(relative)
    confirmed = bool(manifest.get("status") == "installed" and
        manifest.get("package_hashes") == package_hashes and
        manifest.get("runtime_sha256") == digest(runtime) and
        not changes)
    return Plan(repo, app, work, py, manifest_path, manifest, integration,
                changes, source_hashes, package_hashes, confirmed)


_libc = None


def clone_copy(source: Path, destination: Path) -> None:
    """Clone regular files on APFS; otherwise create a new exclusive copy."""
    global _libc
    if destination.exists() or destination.is_symlink():
        raise InstallError(f"Backup destination already exists; refusing to overwrite: {destination}")
    mode = source.lstat().st_mode
    if stat.S_ISLNK(mode):
        destination.symlink_to(os.readlink(source))
    elif stat.S_ISDIR(mode):
        destination.mkdir(mode=stat.S_IMODE(mode))
        for child in source.iterdir():
            clone_copy(child, destination / child.name)
        shutil.copystat(source, destination, follow_symlinks=False)
    elif stat.S_ISREG(mode):
        if _libc is None:
            _libc = ctypes.CDLL("/usr/lib/libSystem.B.dylib", use_errno=True)
            _libc.clonefile.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_int]
            _libc.clonefile.restype = ctypes.c_int
        if _libc.clonefile(os.fsencode(source), os.fsencode(destination), 0):
            code = ctypes.get_errno()
            if code == errno.EEXIST:
                raise InstallError(f"Backup destination appeared during copy: {destination}")
            if code not in (errno.EXDEV, errno.ENOTSUP, errno.EINVAL, errno.ENOSYS):
                raise OSError(code, os.strerror(code), str(source))
            with source.open("rb") as input_file, destination.open("xb") as output_file:
                shutil.copyfileobj(input_file, output_file, 1024 * 1024)
            shutil.copystat(source, destination)
    else:
        raise InstallError(f"Unsupported file type in backup: {source}")


def verify_unchanged(plan: Plan) -> None:
    for relative, expected in plan.source_hashes.items():
        path = target_file(plan.repo, relative)
        actual = digest(path) if path.is_file() else None
        if actual != expected:
            raise InstallError(f"Source changed after validation: {relative}. Backups are retained; retry after edits stop.")
    actual_package = {"patch": digest(PATCH)}
    actual_package.update({name: digest(ROOT / "runtime" / name) for name in HEADERS})
    if actual_package != plan.package_hashes:
        raise InstallError("The editor package changed during installation. Backups are retained; rerun using one complete release.")
    if active_game_processes(plan.app):
        raise InstallError("BOTW started during backup. Quit it and rerun; no source changes have been applied.")


def stream_command(command: list[str], log_path: Path) -> None:
    with log_path.open("w", encoding="utf-8") as log:
        process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        assert process.stdout is not None
        for line in process.stdout:
            log.write(line)
            log.flush()
            print(line, end="", flush=True)
        result = process.wait()
    if result:
        raise InstallError(f"{Path(command[1]).name} stopped with exit {result}. Read {log_path}. The runtime updater retains its own app rollback behavior.")


def cache_manifest(path: Path) -> dict:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
        return value if isinstance(value, dict) and value.get("schema") == 1 else {}
    except (OSError, ValueError):
        return {}


def cached_png(path: Path, checksum: object) -> bool:
    if not isinstance(checksum, str) or not re.fullmatch(r"[0-9a-f]{64}", checksum):
        return False
    try:
        with path.open("rb") as stream:
            if stream.read(8) != b"\x89PNG\r\n\x1a\n":
                return False
        return digest(path) == checksum
    except OSError:
        return False


def map_cache_valid() -> bool:
    folder = APP_DATA / "cache/map"
    metadata = cache_manifest(folder / "map.json")
    bounds = metadata.get("world_bounds")
    if metadata.get("image") != "map.png" or metadata.get("orientation") != "north-up" or \
       not all(isinstance(metadata.get(key), int) and metadata[key] > 0 for key in ("width", "height")) or \
       not isinstance(bounds, dict):
        return False
    coordinates = [bounds.get(key) for key in ("min_x", "max_x", "min_z", "max_z")]
    return all(isinstance(value, (int, float)) and math.isfinite(value) for value in coordinates) and \
        coordinates[1] > coordinates[0] and coordinates[3] > coordinates[2] and \
        cached_png(folder / "map.png", metadata.get("image_sha256"))


def items_cache_valid() -> bool:
    folder = APP_DATA / "cache/items"
    metadata = cache_manifest(folder / "manifest.json")
    icons, catalog = metadata.get("icons"), metadata.get("catalog")
    if not isinstance(icons, dict) or not icons or len(icons) > 10000 or \
       metadata.get("icon_count") != len(icons) or \
       not isinstance(metadata.get("stock_count"), int) or not 0 < metadata["stock_count"] <= len(icons) or \
       not isinstance(catalog, dict):
        return False
    try:
        catalog_ids = {item["id"] for item in json.loads((ROOT / "Sources/catalog.json").read_text())}
    except (OSError, ValueError, KeyError, TypeError):
        return False
    missing = sorted(catalog_ids - icons.keys())
    if catalog.get("total") != len(catalog_ids) or catalog.get("resolved") != len(catalog_ids) - len(missing) or \
       catalog.get("resolved", 0) < 1 or catalog.get("missing") != missing:
        return False
    for actor, icon in icons.items():
        if not re.fullmatch(r"[A-Za-z][A-Za-z0-9_]*(?:\.[0-9]{2})?", actor) or \
           not isinstance(icon, dict) or icon.get("file") != actor + ".png" or \
           not all(isinstance(icon.get(key), int) and icon[key] > 0 for key in ("width", "height")) or \
           not cached_png(folder / icon["file"], icon.get("sha256")):
            return False
    return True


def ensure_private_assets(plan: Plan, log_folder: Path) -> None:
    for kind, validate in (("map", map_cache_valid), ("items", items_cache_valid)):
        if validate():
            say(f"Reusing verified private {kind} cache.")
            continue
        log_folder.mkdir(parents=True, exist_ok=True, mode=0o700)
        say(f"Extracting private {kind} artwork from your local game files…")
        command = [str(plan.python), str(ROOT / f"scripts/extract_{kind}.py"),
                   "--app", str(plan.app), "--output-dir", str(APP_DATA / "cache" / kind)]
        if kind == "items":
            command.extend(["--catalog", str(ROOT / "Sources/catalog.json")])
        stream_command(command, log_folder / f"{kind}-extraction.log")
        if not validate():
            raise InstallError(f"Private {kind} extraction did not produce a complete, hash-verified cache. Read {log_folder / (kind + '-extraction.log')} and retry.")


def install(plan: Plan) -> None:
    destinations = GAME_DATA / "editor-destinations.tsv"
    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S-%f")
    if plan.runtime_confirmed:
        say("Already installed: bridge source and runtime match this release; no runtime rebuild needed.")
        logs = APP_DATA / "logs" / stamp
        try:
            if not destinations.is_file():
                logs.mkdir(parents=True, exist_ok=True, mode=0o700)
                stream_command([str(plan.python), str(ROOT / "scripts/extract_destinations.py"),
                                "--app", str(plan.app), "--output", str(destinations)], logs / "destinations.log")
            ensure_private_assets(plan, logs)
        except Exception as error:
            raise InstallError(f"Runtime is installed, but private asset setup did not finish: {error}. Extraction logs are retained at {logs}; retry to resume without rebuilding.") from error
        say("Private map, item artwork, and destinations are ready. Open BOTW Editor.")
        return
    backup = APP_DATA / "Backups" / stamp
    backup.mkdir(parents=True, exist_ok=False, mode=0o700)
    say(f"Backup folder: {backup}")
    try:
        originals = {}
        for relative in plan.source_hashes:
            source = plan.repo / relative
            originals[relative] = source.is_file()
            if source.is_file():
                destination = backup / "source" / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                clone_copy(source, destination)
        say("Backing up the game app (APFS file clones when available)…")
        clone_copy(plan.app, backup / plan.app.name)
        profile = GAME_DATA / "profile"
        if profile.exists():
            say("Backing up the gameplay profile…")
            clone_copy(profile, backup / "profile")
        if destinations.exists():
            clone_copy(destinations, backup / "editor-destinations.tsv")
        write_json(backup / "restore.json", {
            "schema": 1, "repo": str(plan.repo), "app": str(plan.app),
            "profile": str(profile), "profile_existed": profile.exists(),
            "source_originally_present": originals, "source_sha256": plan.source_hashes,
            "macnativegames_commit": PINNED_COMMIT,
        })
        verify_unchanged(plan)
        if plan.integration == "needs-install":
            checked(["git", "apply", str(PATCH)], cwd=plan.repo)
            say("Integrated runtime hooks:")
            for relative in SOURCE_FILES:
                say("  " + relative)
        for name in HEADERS:
            relative = "games/totk/runtime/" + name
            target = plan.repo / relative
            if plan.source_hashes[relative] != plan.package_hashes[name]:
                temporary = target.with_name(target.name + f".botw-editor-{os.getpid()}.tmp")
                with temporary.open("xb") as stream:
                    stream.write((ROOT / "runtime" / name).read_bytes())
                temporary.replace(target)
                say("Installed bridge source: " + relative)
        owned = {"games/totk/runtime/" + name: plan.package_hashes[name] for name in HEADERS}
        manifest = {
            "schema": 1, "status": "pending", "repo": str(plan.repo), "app": str(plan.app),
            "macnativegames_commit": PINNED_COMMIT, "owned_headers": owned,
            "package_hashes": plan.package_hashes, "last_backup": str(backup),
        }
        write_json(plan.manifest_path, manifest)
        say("Updating the game runtime; leave a quiet build running…")
        stream_command([str(plan.python), str(plan.repo / "update_runtime.py"),
                        str(plan.app), "--work", str(plan.work)], backup / "runtime-update.log")
        manifest.update(status="installed", runtime_sha256=digest(plan.app / "Contents/Resources/totk/build/totk-native"))
        write_json(plan.manifest_path, manifest)
        say("Extracting destinations from your local game files…")
        stream_command([str(plan.python), str(ROOT / "scripts/extract_destinations.py"),
                        "--app", str(plan.app), "--output", str(destinations)], backup / "destinations.log")
        ensure_private_assets(plan, backup)
        say("Installed. Open BOTW, load your game, then open BOTW Editor.")
        say(f"Keep this app/profile restore point: {backup}")
    except Exception as error:
        try:
            (backup / "INSTALL-FAILED.txt").write_text(str(error) + "\n", encoding="utf-8")
        except OSError:
            pass
        raise InstallError(f"Installation did not finish: {error}\nBackups are retained at {backup}. Source integration may remain in place; a retry can resume it. See docs/INSTALLATION.md before restoring.") from error


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, required=True, help="MacNativeGames checkout at the supported revision")
    parser.add_argument("--app", type=Path, required=True, help="Existing Breath of the Wild 1.6.0 app")
    parser.add_argument("--work", type=Path, required=True, help="Original BotW_build work folder")
    parser.add_argument("--check", action="store_true", help="Validate and report source/runtime state without writing or rebuilding")
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        plan = preflight(args)
        say(f"Compatible: BOTW 1.6.0, MacNativeGames {PINNED_COMMIT[:7]}, Python {plan.python}")
        say("Source integration: " + plan.integration)
        say("Runtime installation: " + ("confirmed" if plan.runtime_confirmed else "not recorded for this release"))
        say("Private map cache: " + ("verified" if map_cache_valid() else "missing or needs extraction"))
        say("Private item cache: " + ("verified" if items_cache_valid() else "missing or needs extraction"))
        if plan.changes:
            say("Planned source changes:")
            for relative in plan.changes:
                say("  " + relative)
        if args.check:
            say("Check complete. No files changed; no build started.")
        else:
            install(plan)
        return 0
    except (InstallError, OSError, ValueError) as error:
        print(f"Cannot install bridge: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
