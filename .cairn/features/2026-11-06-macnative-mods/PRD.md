# Manage MacNativeGames mods from BOTW Editor

You can import a mod, enable it, set its priority, and apply it to Breath of the Wild 1.6.0 on Mac. After restarting, you can see a demo change in the game and turn it off again. Accept Switch RomFS packages and BCML's merged Switch exports through the same importer.

- Stack: SwiftUI and AppKit on macOS 26, the existing Python 3.14 install environment, and MacNativeGames at `b1a49a2`.
- Branch: `feat/macnative-mods`, based on the editor's 0.1.3 trainer update.
- Size: subsystem. The backend, native UI, and demo generation run in parallel; the director reviews and checks the combined flow.
- Use the existing Slate controls and typography. Keep the mod library available while BOTW is closed.

## Where the work lives

- `Sources/BOTWPlayground.swift` owns the window, tabs, and Slate controls. `Sources/UpscalingModel.swift` shows how to distinguish configured settings from what a game launch actually uses.
- `scripts/install-bridge.py` verifies the pinned checkout and records the app and repository paths in a local installation record.
- MacNativeGames `tools/builder/mod_add.py`, `mod_find.py`, `mod_archive.py`, `mod_library.py`, and `mod_tools.py` import, inspect, and order local packs without the Builder GUI.
- MacNativeGames `games/totk/scripts/mod_folder.py` and `make_mod_manifest.py` validate files against the installed app and prepare a verified overlay. Its `games/totk/app/launcher.m` selects the version folder, and `games/totk/runtime/romfs.c` loads the manifest on boot.
- `scripts/build.sh` bundles the app; `scripts/test.sh` compiles the native UI and runs the existing model, bridge, and GPU checks.

## Approach

Use the existing MacNativeGames RomFS loader. Keep a private library for the editor, pass each import through the pinned upstream validators, and let the last enabled pack win for shared file paths. Apply only while the game is closed. Keep a backup of the previous overlay and show that a restart is needed. The setup record locates the existing checkout; the editor adds no runtime dependency.

The Builder could expose its hidden 1.6.0 path with fewer changes, but that would leave players switching apps. A new loader would duplicate a working runtime and require changes to the signed game app. The editor tab reuses the loader and gives the user one place to manage mods and live edits.

BCML's Switch export produces complete replacement files; raw BNP merge logs need a separate merger and the correct game dump. Detect Wii U packages, unsupported DLC-title overlays, and code-only mods. Report the limitation before an apply can silently drop files. No downloads, console writes, game app edits, or code patching in this feature.

Generate two small demos from the player's installed game. Ship the recipes; keep the generated game assets in the private library. The first target is BOTW 1.6.0. Other MacNativeGames versions and games can use the same approach when their app detection, title IDs, overlays, and manifests pass their own checks.

## Verify

- `bash scripts/build.sh` runs the repository build and tests. `xcrun swiftc -typecheck` with the app source list checks the native UI. Check shell syntax, Python syntax, and `git diff --check`.
- Capture the actual AppKit view at 1000 × 680 and 1440 px wide. The editor's minimum width is 1000 px.
- Import local fixtures in Switch, emulator, and BCML-export layouts. Verify conflict order, invalid package refusal, backups, and safe disable. Boot a copy of the save against the demo overlay and check what the game reads and shows.
- Compare the kept save files and the signed game app before and after mod checks. Keep live screenshots private.
