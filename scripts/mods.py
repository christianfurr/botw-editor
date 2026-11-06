#!/usr/bin/env python3
"""JSON command line for the BOTW Editor's private Switch RomFS mod library."""

from __future__ import annotations

import argparse
from contextlib import redirect_stderr, redirect_stdout
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile

# This script can run from the signed editor app's Resources directory.
sys.dont_write_bytecode = True

from mod_environment import (
    ModError, RESOURCES, TITLE_ID, VERSION, editor_data, ensure_dir, game_data,
    game_running, installation, lock_file, native_game_running, overlay_signature,
    present, regular,
)
from mod_import import REFUSALS, check_composition_paths, demos, import_mod, inspect_tree


LIBRARY_MARKER = ".botw-editor-library-v1"
MOD_ID = re.compile(r"[a-z0-9][a-z0-9-]{0,79}\Z")
VALIDATION = {
    "no_romfs": "No RomFS data.",
    "no_app_romfs": "The installed game is missing its RomFS index.",
    "misspelt": "A file name differs from the game's name only by letter case.",
    "other_version": "A versioned file targets a different game version.",
    "only_adds": "This mod does not replace any file in the installed game.",
    "all_taken": "The app already supplies the files this mod would replace.",
}


class JSONParser(argparse.ArgumentParser):
    def error(self, message):
        raise ModError("Invalid mod command or arguments.")


class Backend:
    def __init__(self, install):
        os.environ["PYTHONDONTWRITEBYTECODE"] = "1"
        sys.path.insert(0, str(install.repo))
        from tools.builder import mod_add, mod_applied, mod_archive, mod_find, mod_library, mod_tools
        from tools.builder.ports import CommandRunner, RealClock

        self.install = install
        self.add, self.applied, self.archive, self.find = mod_add, mod_applied, mod_archive, mod_find
        self.lib, self.tools = mod_library, mod_tools
        self.runner = CommandRunner(RealClock())
        manifest, self.folder = mod_tools.tools(install.repo)
        mod_library.TITLE_IDS[VERSION] = TITLE_ID
        self.load = self.folder.mods_folder(VERSION)
        self.slot = game_data() / "game-slot/lock"
        self.root = editor_data() / "mod-library" / VERSION
        self.library = None
        romfs = install.app / RESOURCES / "extracted" / VERSION / "romfs"
        manifest.IMAGE_DIR = None
        if not (romfs / "romfs.bin").is_file():
            manifest.IMAGE_DIR = manifest.app_image_dir(install.app, VERSION)
            if not manifest.image_file(romfs).is_file():
                raise ModError("Open BOTW once to locate the game data folder, then reopen the mod library.")

    def open_library(self):
        ensure_dir(self.root)
        marker = self.root / LIBRARY_MARKER
        if not present(marker):
            if any(self.root.iterdir()):
                raise ModError("The editor mod library already contains unrecognized files.")
            with marker.open("x", encoding="utf-8") as stream:
                stream.write("BOTW Editor Mod Library 1\n")
        if not regular(marker) or marker.read_text() != "BOTW Editor Mod Library 1\n":
            raise ModError("The editor mod library ownership marker is invalid.")
        for leftover in (self.add.UNZIP, self.lib.STATE + ".partial"):
            if present(self.root / leftover):
                raise ModError("The mod library has unfinished staging files. Set them aside before retrying.")
        state_path = self.root / self.lib.STATE
        if present(state_path):
            if not regular(state_path):
                raise ModError("The mod library record is invalid.")
            state = json.loads(state_path.read_text())
            if not isinstance(state, dict) or state.get("format") != 1:
                raise ModError("The mod library record is invalid.")
            order, enabled = state.get("order"), state.get("enabled")
            if (not isinstance(order, list) or any(not isinstance(i, str) or not MOD_ID.fullmatch(i) for i in order)
                    or len(set(order)) != len(order) or not isinstance(enabled, list)
                    or any(not isinstance(i, str) or i not in order for i in enabled) or len(set(enabled)) != len(enabled)):
                raise ModError("The mod library record contains invalid mod entries.")
            for mod_id in order:
                folder = self.root / mod_id
                if not folder.is_dir() or folder.is_symlink():
                    raise ModError("An imported mod folder is missing or invalid.")
                inspect_tree(folder)
        self.library = self.lib.open_library(editor_data(), VERSION, self.load)

    def blocked(self):
        keyboard = self.load.with_name(VERSION + "-keyboard") / "manifest.tsv"
        if present(keyboard):
            return "The keyboard-specific BOTW overlay can take priority. Set aside mods/botw160-keyboard before changing the editor's mod library."
        manifest = self.load / "manifest.tsv"
        if self.load.is_symlink() or (present(self.load) and not self.load.is_dir()) or (present(manifest) and not regular(manifest)):
            return "Set aside the existing overlay. BOTW needs a real mod folder with a regular manifest."
        return None

    def state(self, ok, message, **extra):
        overlay, count = overlay_signature(self.load)
        library = self.library
        key = self.applied.applied_key(library, (self.install.runtime_hash, overlay))
        pending = library.state["applied_failed"] or (
            (library.state["applied"] is not None or library.state["enabled"])
            and library.state["applied"] != key
        )
        return {
            "ok": ok, "message": message, "version": VERSION,
            "game_running": game_running(self.slot), "pending": bool(pending),
            "applied_files": count, "blocked_reason": self.blocked(),
            "mods": [
                {"id": mod.id, "name": mod.name, "enabled": mod.enabled,
                 "valid": mod.validation.valid,
                 "detail": VALIDATION.get(mod.validation.reason, "Ready for Switch RomFS") + (
                     " ExeFS patches and cheats are ignored." if mod.validation.code == "not_for_game"
                     or {"exefs_ignored", "cheats_ignored"}.intersection(mod.notes) else ""),
                 "replaced": mod.validation.replaced, "added": mod.validation.added,
                 "conflicts": list(mod.conflicts)}
                for mod in self.lib.mods(library)
            ],
            **extra,
        }

    def remove(self, mod_id):
        # Retain the imported copy, including any files added after import.
        recovery = Path(tempfile.mkdtemp(prefix=".removed-", dir=self.root))
        destination = recovery / mod_id
        self.library.path(mod_id).rename(destination)
        try:
            self.lib.remove(self.library, mod_id)
        except Exception:
            destination.rename(self.library.path(mod_id))
            recovery.rmdir()
            raise
        return "Removed from the library. A local recovery copy is kept."

    def apply(self):
        ensure_dir(self.load.parent)
        ensure_dir(self.slot.parent)
        try:
            with lock_file(self.slot, create=True):
                if native_game_running():
                    raise ModError("Quit BOTW before applying mods.")
                if blocked := self.blocked():
                    raise ModError(blocked)
                return self.apply_locked()
        except BlockingIOError as error:
            raise ModError("Quit BOTW before applying mods.") from error

    def apply_locked(self):
        library = self.library
        if not library.state["enabled"] and library.state["applied"] is None:
            raise ModError("Enable a mod before applying. Any existing overlay stays in place.")
        check_composition_paths(library.path(mod_id) / "romfs" for mod_id in library.state["enabled"])
        before = self.load.lstat() if present(self.load) else None
        previous_overlay = overlay_signature(self.load)
        backup = None
        with tempfile.TemporaryDirectory(prefix=".botw-editor-stage-", dir=self.load.parent) as directory:
            scratch = Path(directory)
            source = scratch / "source"
            for mod_id in library.state["enabled"]:
                check = self.tools.validate(self.install.repo, self.install.app, VERSION, library.path(mod_id))
                if not check.valid:
                    raise ModError("An enabled mod no longer matches this app. Disable it or import it again before applying.")
                shutil.copytree(library.path(mod_id) / "romfs", source / "romfs",
                                dirs_exist_ok=True, copy_function=self.lib.clone_file)
            staged = scratch / VERSION
            if library.state["enabled"]:
                # Scope the upstream installer's fixed .new path to private staging.
                original = self.folder.mods_folder
                self.folder.mods_folder = lambda version: staged
                try:
                    installed = self.folder.install(source, VERSION, self.install.app, sys.executable,
                                                    self.install.repo / "games/totk", say=lambda _: None)
                finally:
                    self.folder.mods_folder = original
                if installed is None:
                    raise ModError("The enabled mods produced no loadable files. Review the selected mods.")
            if ((self.load.lstat() if present(self.load) else None) != before
                    or overlay_signature(self.load) != previous_overlay):
                raise ModError("The active overlay changed while staging mods. Try again.")
            if blocked := self.blocked():
                raise ModError(blocked)
            if native_game_running():
                raise ModError("Quit BOTW before applying mods.")
            previous_key, previous_failed = library.state["applied"], library.state["applied_failed"]
            if before is not None:
                backup = self.folder.backup_name(self.load)
                self.load.rename(backup)
            installed = False
            try:
                if staged.exists():
                    staged.rename(self.load)
                    installed = True
                overlay, count = overlay_signature(self.load)
                library.state["applied"] = self.applied.applied_key(library, (self.install.runtime_hash, overlay))
                library.state["applied_failed"] = False
                self.lib.save(library)
            except Exception:
                library.state["applied"], library.state["applied_failed"] = previous_key, previous_failed
                if installed:
                    self.load.rename(staged)
                if backup is not None and not present(self.load):
                    backup.rename(self.load)
                raise
        message = f"Applied {count} RomFS files. Open BOTW to load them." if count else "Mods are disabled for the next BOTW launch."
        return message, {"backup": backup.name} if backup is not None else {}

    def run(self, args):
        action = args.action
        if action == "status":
            return "Mod library ready.", {}
        if blocked := self.blocked():
            raise ModError(blocked)
        if action == "import":
            return import_mod(self, args.path), {}
        if action == "demos":
            return demos(self), {}
        if action == "apply":
            return self.apply()
        if args.id not in self.library.state["order"]:
            raise ModError("The selected mod is no longer in the library.")
        if action == "toggle":
            if args.setting == "on" and not self.lib.validation(self.library, args.id).valid:
                raise ModError("This mod is invalid for the installed game.")
            self.lib.set_enabled(self.library, args.id, args.setting == "on")
        elif action == "move":
            self.lib.move(self.library, args.id, -1 if args.direction == "up" else 1)
        elif action == "remove":
            return self.remove(args.id), {}
        return "Selection saved. Choose Apply to update the next game launch.", {}


def arguments(argv):
    parser = JSONParser()
    parser.add_argument("--installation", type=Path)
    commands = parser.add_subparsers(dest="action", required=True, parser_class=JSONParser)
    for action in ("status", "apply", "demos"):
        commands.add_parser(action)
    commands.add_parser("import").add_argument("path", type=Path)
    toggle = commands.add_parser("toggle")
    toggle.add_argument("id")
    toggle.add_argument("setting", choices=("on", "off"))
    move = commands.add_parser("move")
    move.add_argument("id")
    move.add_argument("direction", choices=("up", "down"))
    commands.add_parser("remove").add_argument("id")
    return parser.parse_args(argv)


def execute(args):
    backend = Backend(installation(args.installation))
    ensure_dir(backend.root.parent)
    try:
        with lock_file(backend.root.parent / ".lock", create=True):
            backend.open_library()
            try:
                message, extra = backend.run(args)
                return backend.state(True, message, **extra)
            except backend.find.Refused as error:
                return backend.state(False, REFUSALS.get(error.reason, "The mod package could not be safely imported."))
            except backend.folder.InstallError:
                return backend.state(False, "The game rejected the staged mod files. Check for wrong-case names or conflicting assets.")
            except ModError as error:
                return backend.state(False, str(error))
            except (OSError, ValueError):
                return backend.state(False, "Could not finish the mod operation. Check free space and file permissions.")
    except BlockingIOError as error:
        raise ModError("The mod library is busy. Wait for the current operation to finish.") from error


def main(argv=None):
    try:
        args = arguments(argv)
        with open(os.devnull, "w") as sink, redirect_stdout(sink), redirect_stderr(sink):
            result = execute(args)
    except ModError as error:
        result = {"ok": False, "message": str(error)}
    except Exception:
        result = {"ok": False, "message": "The mod library could not be opened. Verify the editor bridge and retry."}
    print(json.dumps(result, ensure_ascii=True))
    return 0 if result["ok"] else 1


if __name__ == "__main__":
    sys.exit(main())
