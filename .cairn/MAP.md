---
verified_at_sha: 3a09e5a
verified_at: 2026-11-08
---

# BOTW Editor

Native macOS companion for editing a running MacNativeGames Breath of the Wild 1.6.0 session.
This repository contains the editor and bridge source, never the game or a player's data.

## Read first

- [DECISIONS.md](DECISIONS.md): compatibility and mutation boundaries.
- [README.md](../README.md): installation and user-facing behavior.
- [Mods](../docs/MODS.md): Switch formats, BCML export, demos, and recovery.
- [Architecture](../docs/ARCHITECTURE.md): command acknowledgement and runtime bridge.
- [Testing](../docs/TESTING.md): evidence and live verification requirements.
- [First stable release](../docs/FIRST-RELEASE.md): readiness checklist, live recovery evidence, and signing steps.

## Source layout

- `Sources/BOTWPlayground.swift`: Inventory/Player/World views, Give grid, sword levels, capacity sheet, separate map/activation/clear actions, horse control and zoom-dependent map labels.
- `Sources/PlaygroundModel.swift`: session-bound acknowledgement, live/readiness gates, item rules, bulk armor grants, and the item catalog. `tests/trainer_session_checks.swift` checks stale replies, reconnects, and process exit.
- `Sources/ModsModel.swift` and `Sources/ModsView.swift`: standalone MacNative Mods views and command model. `Sources/MacNativeMods.swift` selects installed game apps.
- `scripts/mods.py`, `mod_environment.py`, and `mod_import.py`: separate per-version libraries, legacy BOTW import, checked composition, game-slot exclusion and recoverable overlay swaps.
- `scripts/mod_demos.py`: exact-size demo resources generated from the installed game; no retail files in the bundle.
- `Sources/GraphicsModel.swift`: color/custom-shader settings and presenter acknowledgement.
- `Sources/UpscalingModel.swift` and `GraphicsSettingsView.swift`: separate restart-required upscaling controls, immutable boot-preset confirmation, and actual GPU/dimension status.
- `runtime/`: nine managed bridge/presenter/config headers, the six-source-file fresh patch, and separate legacy-to-0.1.3 / 0.1.2-to-0.1.3 upgrade patches.
- `runtime/botw_editor_horse.h`: guarded shared-table land-gear changes, baseline/ownership checks, rollback and session-only Off restoration.
- `runtime/botw_editor_runes.h`: guarded nonblocking request of an absent rune selection; native updates apply/persist it, with separate requested/current/saved diagnostics.
- `scripts/install-bridge.py`: pinned compatibility preflight, source ownership checks, app/profile backups, runtime update, and private resource extraction.
- `scripts/extract_destinations.py`: builds a private 135-location catalog from the player's installed game.
- `scripts/extract_map.py` and `scripts/extract_items.py`: decode the player's terrain tiles and item icons into Application Support; never into the repository or release app.
- `scripts/extract_map_labels.py` / `test_map_labels.py`: private original-position label extraction and eight synthetic validation fixtures.
- `scripts/test_install_bridge.py`: 33 fixtures for guards, backups, ownership, fresh/legacy/0.1.2 patch selection and private cache/label setup/reuse; does not open the game.
- `tests/runtime_checks.c` and `scripts/test-runtime.sh`: 29,636 strict native guards, including sword/capacity/Slate and mixed shrine-discovery/clear transactions; no gameplay.
- `tests/horse_checks.c` and `scripts/test-horse.sh`: 16,113 ASan/UBSan assertions with synthetic resources and injected ownership/write failures.
- `scripts/test-postfx.sh` and `tests/postfx_gpu.m`: 325 synthetic GPU pixel assertions; explicit CI skip only when no Metal device exists.
- `scripts/test-temporal.sh`, `tests/temporal_config_checks.c`, and `tests/temporal_gpu.m`: ASan/UBSan startup/draw/ordering guards and a 24-completed-frame real MetalFX probe.
- `tests/upscaling_model_checks.swift`: 90 preset/I/O/status/restart assertions.
- `scripts/test.sh`: Swift/model/UI checks, core/horse native assertions, 249 temporal guards, GPU probes, eight label fixtures and 33 installer checks.
- `scripts/build-mod-manager.sh`: standalone manager app and ZIP.
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
The map combines private terrain, 135 destinations, 557 extracted place labels and the player's position. Labels declutter by zoom; their data and artwork stay private. Full Access raises towers and skips the opening tower sequence; activation/discovery remains separate from explicit clearing.
The 0.1.3 bridge supports canonical unequipped sword levels, increase-only capacity, two-flag Slate access, nine rune/Slate flags plus a separate selection request, typed shrine discovery/clear transactions and session horse speeds. Each capability is independently gated; older snapshots still decode. `docs/TESTING.md` records the flags-only rune failure, visible selector and Bomb verification and the player's deliberately kept profile separately from other live/artifact evidence.

## Public repository boundary

Do not add keys, firmware, NSP/XCI/NCA files, extracted game resources, saves, user logs,
memory dumps, machine-specific paths, or gameplay screenshots. Release ZIPs contain the editor app only.
The bridge installer operates on a player's existing local MacNativeGames installation and keeps backups.
