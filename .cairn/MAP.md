---
verified_at_sha: uncommitted
verified_at: 2026-11-04
---

# BOTW Editor

Native macOS companion for editing a running MacNativeGames Breath of the Wild 1.6.0 session.
This repository contains the editor and bridge source, never the game or a player's data.

## Read first

- [DECISIONS.md](DECISIONS.md): compatibility and mutation boundaries.
- [README.md](../README.md): installation and user-facing behavior.
- [Architecture](../docs/ARCHITECTURE.md): command acknowledgement and runtime bridge.
- [Testing](../docs/TESTING.md): evidence and live verification requirements.

## Source layout

- `Sources/`: SwiftUI Inventory/Player/World editor, connection model, and bundled item-name catalog.
- `runtime/`: five version-specific bridge headers and the two-file `macnativegames-botw160.patch` integration.
- `scripts/install-bridge.py`: pinned compatibility preflight, source ownership checks, app/profile backups, runtime update, and private resource extraction.
- `scripts/extract_destinations.py`: builds a private 135-location catalog from the player's installed game.
- `scripts/extract_map.py` and `scripts/extract_items.py`: decode the player's terrain tiles and item icons into Application Support; never into the repository or release app.
- `scripts/test_install_bridge.py`: 12 fixture checks for guards, backups, ownership, patch idempotence, and private-cache setup/reuse; does not open the game.
- `tests/runtime_checks.c` and `scripts/test-runtime.sh`: strict native guard, parser, queue, and synthetic live-pointer checks; no game data or gameplay.
- `scripts/test.sh`: combined Swift model, native runtime, and installer fixture checks.
- `scripts/build.sh`: arm64 app/archive packaging, native icon generation, bundled notices, ad hoc signing, and checksum output.
- `scripts/generate-icon.swift`: original vector app icon, rendered with AppKit.
- `Assets/`: original editor artwork and editor-only screenshots. Public screenshots must omit extracted terrain/item artwork; the Player tab is suitable.
- `docs/`: architecture, installation recovery, and verification evidence.

## Verification boundary

Compiled/model checks validate protocol and guard behavior. They do not prove that a game edit works.
Live evidence must identify the game version, operation, acknowledgement, readback, and visible result when relevant.
Live checks for core edits, exact weapon-durability readbacks, world access, three travel destinations, and save/reload are recorded in `docs/TESTING.md`. UI point-to-unit conversion has separate compiled exact-hundredths checks.
The map displays locally extracted terrain with 120 base shrines, 15 towers, and the player's position. Extracted artwork stays in the player's private cache. Full Access raises towers and skips the opening tower activation sequence; shrine activation does not mark a shrine clear.

## Public repository boundary

Do not add keys, firmware, NSP/XCI/NCA files, extracted game resources, saves, user logs,
memory dumps, machine-specific paths, or gameplay screenshots. Release ZIPs contain the editor app only.
The bridge installer operates on a player's existing local MacNativeGames installation and keeps backups.
