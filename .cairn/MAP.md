---
verified_at_sha: 18c1478b694b6caab161b2754b48ad6bfeb9f8c0
verified_at: 2026-11-05
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

- `Sources/BOTWPlayground.swift`: compact Inventory/Player/World views and themed icon-grid Give sheet.
- `Sources/PlaygroundModel.swift`: request acknowledgement, live/readiness gates, and item-name catalog.
- `Sources/GraphicsModel.swift`: color/custom-shader settings and presenter acknowledgement.
- `Sources/UpscalingModel.swift` and `GraphicsSettingsView.swift`: separate restart-required upscaling controls, immutable boot-preset confirmation, and actual GPU/dimension status.
- `runtime/`: seven managed bridge/presenter/config headers, the six-source-file `macnativegames-botw160.patch`, and the atomic upgrade compatibility patch `macnativegames-v0.1.1-to-v0.1.2.patch`.
- `scripts/install-bridge.py`: pinned compatibility preflight, source ownership checks, app/profile backups, runtime update, and private resource extraction.
- `scripts/extract_destinations.py`: builds a private 135-location catalog from the player's installed game.
- `scripts/extract_map.py` and `scripts/extract_items.py`: decode the player's terrain tiles and item icons into Application Support; never into the repository or release app.
- `scripts/test_install_bridge.py`: 24 fixture checks for guards, backups, ownership, fresh/0.1.0/0.1.1 atomic patch selection, and private-cache setup/reuse; does not open the game.
- `tests/runtime_checks.c` and `scripts/test-runtime.sh`: strict native guard, parser, queue, and synthetic live-pointer checks; no game data or gameplay.
- `scripts/test-postfx.sh` and `tests/postfx_gpu.m`: 325 synthetic GPU pixel assertions; explicit CI skip only when no Metal device exists.
- `scripts/test-temporal.sh`, `tests/temporal_config_checks.c`, and `tests/temporal_gpu.m`: ASan/UBSan startup/draw/ordering guards and a 24-completed-frame real MetalFX probe.
- `tests/upscaling_model_checks.swift`: 90 preset/I/O/status/restart assertions.
- `scripts/test.sh`: combined Swift model/UI/graphics/upscaling, 224 native assertions, temporal guards, GPU probes and installer checks.
- `scripts/build.sh`: arm64 app/archive packaging, native icon generation, bundled notices, ad hoc signing, and checksum output.
- `scripts/generate-icon.swift`: original vector app icon, rendered with AppKit.
- `Assets/`: original editor artwork and editor-only screenshots. Public screenshots must omit extracted terrain/item artwork; the Player and Graphics views are suitable.
- `docs/`: architecture, installation recovery, and verification evidence.

## Verification boundary

Compiled/model checks validate protocol and guard behavior. They do not prove that a game edit works.
Live evidence must identify the game version, operation, acknowledgement, readback, and visible result when relevant.
The dated 0.1.0 checks for core edits, durability readbacks, world access, three travel destinations, and save/reload remain in `docs/TESTING.md`. The 0.1.1 checks now include undyed-armor readiness, grants with system/Materials menus open, a visible custom grayscale shader, and invalid-source fallback/recovery; source/synthetic checks are separate evidence.
The current inventory scan accepts armor’s -1 no-dye sentinel; per-type weapon/stack bounds remain strict. `inventory_ready` and `inventory_block_reason` distinguish real blockers without claiming every menu must close.
`botw_postfx.h` filters the final SDR presentation through Off/Natural/Vivid/Cinematic/Custom Metal paths; Off and shader errors use the original image. [Shader contract](../docs/SHADERS.md).
The 0.1.2 final runtime completed 3,788 GPU temporal frames at 4K with zero GPU errors, camera/walk input and menu/resume. The recorded run retained one initial history reset and one menu-resume reset; `docs/TESTING.md` states its duration and limits. See [temporal details](../docs/TEMPORAL.md).
Five canonical boot presets drive both guest render dimensions and temporal output. The UI writes `graphics-upscaler.txt` only on explicit save, requires a restart, and distinguishes selected intent from fresh GPU-completed status. Motion is camera-derived and experimental.
The map displays locally extracted terrain with 120 base shrines, 15 towers, and the player's position. Extracted artwork stays in the player's private cache. Full Access raises towers and skips the opening tower activation sequence; shrine activation does not mark a shrine clear.

## Public repository boundary

Do not add keys, firmware, NSP/XCI/NCA files, extracted game resources, saves, user logs,
memory dumps, machine-specific paths, or gameplay screenshots. Release ZIPs contain the editor app only.
The bridge installer operates on a player's existing local MacNativeGames installation and keeps backups.
