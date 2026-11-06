"""Validate the installed Switch app and coordinate with its native game slot."""

from contextlib import contextmanager
from dataclasses import dataclass
import fcntl
import hashlib
import json
import os
from pathlib import Path
import plistlib
import stat
import subprocess

PINNED_COMMIT = "b1a49a26650eb31a986e7841f449bb44446c5208"
VERSION = "botw160"
TITLE_ID = "01007EF00011E000"
RESOURCES = Path("Contents/Resources/totk")


class ModError(Exception):
    pass


@dataclass(frozen=True)
class Installation:
    repo: Path
    app: Path
    runtime_hash: str


def editor_data():
    return Path.home() / "Library/Application Support/BOTWEditor"


def game_data():
    return Path.home() / "Library/Application Support/MacNativeGames/BotW"


def present(path):
    return path.exists() or path.is_symlink()


def regular(path):
    return stat.S_ISREG(path.lstat().st_mode)


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def verified_record(path):
    if not regular(path) or path.stat().st_size > 1 << 20:
        raise ModError("The bridge installation record is invalid.")
    record = json.loads(path.read_text())
    if (not isinstance(record, dict) or type(record.get("schema")) is not int or record["schema"] != 1
            or record.get("status") != "installed" or record.get("macnativegames_commit") != PINNED_COMMIT):
        raise ModError("The mod library requires an installed BOTW 1.6.0 bridge.")
    repo, app = (Path(record[key]).expanduser() for key in ("repo", "app"))
    if not all(path.is_absolute() and path.is_dir() for path in (repo, app)):
        raise ModError("The bridge record points to a missing checkout or game app.")
    repo, app = repo.resolve(), app.resolve()
    git = subprocess.run(["/usr/bin/git", "-C", str(repo), "rev-parse", "HEAD"],
                         capture_output=True, text=True, timeout=10)
    if git.returncode or git.stdout.strip() != PINNED_COMMIT:
        raise ModError("The MacNativeGames checkout must match the bridge's supported revision.")
    tools = ("tools/builder", "tools/__init__.py", "games/totk/scripts/make_mod_manifest.py",
             "games/totk/scripts/mod_folder.py", "games/totk/scripts/make_romfs_patches.py",
             "games/totk/scripts/make_botw_button_icons.py")
    status = subprocess.run(["/usr/bin/git", "--no-optional-locks", "-C", str(repo), "status", "--porcelain",
                             "--untracked-files=all", "--", *tools], capture_output=True, timeout=10)
    if status.returncode or status.stdout:
        raise ModError("Restore the original MacNativeGames mod tools before using the editor's mod library.")
    info = plistlib.loads((app / "Contents/Info.plist").read_bytes())
    if not isinstance(info, dict) or info.get("CFBundleIdentifier") != "org.macnativegames.botw" or info.get("CFBundleShortVersionString") != "1.6.0":
        raise ModError("Choose the MacNativeGames Breath of the Wild 1.6.0 app.")
    extracted = app / RESOURCES / "extracted"
    versions = {item.name for item in extracted.iterdir() if item.is_dir()} - {"firmware"}
    if versions != {VERSION} or not regular(extracted / VERSION / "romfs/runtime-index.bin"):
        raise ModError("The installed app does not contain a BOTW 1.6.0 RomFS index.")
    runtime = app / RESOURCES / "build/totk-native"
    runtime_hash = record.get("runtime_sha256")
    if not regular(runtime) or digest(runtime) != runtime_hash:
        raise ModError("The game runtime changed since the bridge was installed. Re-run the bridge installer.")
    return Installation(repo, app, runtime_hash)


def installation(record):
    invalid = (OSError, TypeError, ValueError, KeyError, plistlib.InvalidFileException, subprocess.TimeoutExpired)
    if record is not None:
        try:
            return verified_record(record)
        except invalid as error:
            raise ModError("The specified bridge installation record could not be verified.") from error
    records = editor_data() / "Installations"
    if records.is_dir() and not records.is_symlink():
        candidates = [path for path in records.glob("*.json") if path.is_file() and not path.is_symlink()]
        for path in sorted(candidates, key=lambda path: path.stat().st_mtime_ns, reverse=True):
            try:
                return verified_record(path)
            except (ModError, *invalid):
                continue
    raise ModError("Install the BOTW 1.6.0 editor bridge before opening the mod library.")


def ensure_dir(path):
    if path.is_symlink() or (path.exists() and not path.is_dir()):
        raise ModError("A mod support folder is not a real directory.")
    path.mkdir(mode=0o700, parents=True, exist_ok=True)


@contextmanager
def lock_file(path, *, create):
    flags = (os.O_RDWR | os.O_CREAT) if create else os.O_RDONLY
    descriptor = os.open(path, flags | os.O_NOFOLLOW | os.O_NONBLOCK, 0o600)
    try:
        details = os.fstat(descriptor)
        if not stat.S_ISREG(details.st_mode) or details.st_nlink != 1:
            raise ModError("The mod lock file is invalid.")
        fcntl.flock(descriptor, fcntl.LOCK_EX | fcntl.LOCK_NB)
        current = path.lstat()
        if (current.st_dev, current.st_ino) != (details.st_dev, details.st_ino):
            raise ModError("The mod lock changed while opening it. Try again.")
        yield
    finally:
        os.close(descriptor)


def native_game_running():
    # Direct development launches may use a different game-slot directory.
    result = subprocess.run(["/bin/ps", "-axo", "comm="], capture_output=True, text=True, timeout=10)
    if result.returncode:
        raise ModError("Could not check whether the game is running.")
    return any(Path(line.strip()).name.startswith("totk-native") for line in result.stdout.splitlines())


def game_running(slot):
    try:
        with lock_file(slot, create=False):
            return native_game_running()
    except BlockingIOError:
        return True
    except FileNotFoundError:
        return native_game_running()


def overlay_signature(folder):
    manifest = folder / "manifest.tsv"
    if folder.is_symlink() or (present(manifest) and not regular(manifest)):
        return None, 0
    if not manifest.is_file():
        return None, 0
    with manifest.open(encoding="utf-8", errors="replace") as stream:
        count = sum(bool(line.strip()) and not line.startswith("#") for line in stream)
    checksum = hashlib.sha256(digest(manifest).encode())
    # Detect files replaced or edited outside the editor, even when the
    # manifest still has the same text. A status read does not hash the
    # potentially large mod assets or claim the game loaded them.
    for root, dirs, files in os.walk(folder, followlinks=False):
        dirs.sort()
        for name in sorted(dirs + files):
            path = Path(root) / name
            st = path.lstat()
            entry = (path.relative_to(folder).as_posix(), st.st_dev, st.st_ino,
                     st.st_mode, st.st_size, st.st_mtime_ns, st.st_ctime_ns)
            checksum.update(json.dumps(entry).encode())
    return checksum.hexdigest(), count
