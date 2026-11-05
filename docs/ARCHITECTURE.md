# Architecture

BOTW Editor has two local components: a SwiftUI companion app and a small bridge compiled into the player's MacNativeGames runtime.

## Connection and commands

The editor reads `trainer-state.json` beside the game's control files under `~/Library/Application Support/MacNativeGames/BotW/`. State includes the game process ID, protocol schema, game version, available features, live values, inventory, destinations, and the last request result.

Before allowing an edit, the companion checks that the process is alive, the state is no more than 15 seconds old, the game version is supported, the command file exists, and the requested feature is available. Maximum-stat edits also wait for `player_cache_ready` when the bridge supplies that field. The displayed maxima prefer `max_hearts_live` and `max_stamina_live` over the saved GameData values.

During travel, `teleport_busy` disables mutations and the companion shows a loading state. A request acknowledgement confirms acceptance or an applied edit according to its result; it does not establish arrival at a destination. Live verification must check loading completion, actual position, and restored player control.

Each request has a unique ID and is appended to `input.txt`. The command-file thread validates and queues it. The bridge applies a queued mutation on the game's presentation thread, where native game functions have the runtime context they need. A matching acknowledgement and readback determine the displayed result. The bridge publishes state through an atomic file replacement so the companion does not consume partial JSON.

## Version-specific bridge

The initial target is Switch BOTW 1.6.0 on MacNativeGames commit `b1a49a2`. The bridge checks relevant instruction signatures, mapped pointers, object layouts, numeric ranges, and current session state. Unknown layouts remain unavailable.

- Inventory operations use the game's pouch-manager APIs and synchronize GameData rather than rewriting a save file behind the game.
- World flags use native typed GameData getters/setters. The bridge recognizes both direct flags and the game's flag proxies.
- The health lock holds current health; the stamina lock refills stamina. Both start off and stop when their gameplay context disappears.
- Shrine activation enables travel access without marking the shrine clear. Full Access sets 144 travel/ability flags, including `MapTower_07_Demo`, which raises the world towers and skips the opening tower activation sequence. It leaves shrine-clear and story-quest completion fields alone.
- Travel uses the game's native reset route from an outdoor `MainField` session. The bridge rejects unsupported interior contexts and unavailable tower destinations. See the live verification record for outcome-specific limits.

## Local map, destinations, and item icons

The installer extracts 120 base-shrine and 15 tower destinations from the player's game. It also decodes the player's terrain tiles and item icons into `~/Library/Application Support/BOTWEditor/cache/`. No extracted artwork enters the source tree or app bundle.

The companion projects those 135 destination markers and the current player position over the local terrain image, with north at the top. Negative world Z maps toward the top of the canvas. Terrain bounds remain fixed when search filters change, and the view letterboxes the map rather than stretching its projection. Selecting a marker or row chooses a known destination; the UI does not provide arbitrary click-to-coordinate travel. [Format and coordinate details](MAP-EXTRACTION.md)

Item tiles load icons from the local cache by validated actor identifier. Game metadata can resolve shared icons for upgraded or variant actors; catalog-only legacy mappings affect the icon, not whether the native game will accept an item grant. Missing artwork uses system-symbol fallbacks.

The destination's optional `teleportable` field controls its travel action. A false value disables the button and explains how to raise the towers. `towers_raised` provides the current tower state. Older bridge snapshots that omit these optional fields retain the earlier feature gates.

## Upscaling and color filters

`UpscalingModel` controls a restart-required preset shared by the scene-resolution
adapter and MetalFX temporal renderer. It writes the canonical token only after
**Use on next launch**. The UI separates saved intent from a live process’s
immutable boot token, actual dimensions, completed GPU work, and fallback state.
See [Temporal reconstruction](TEMPORAL.md) for all five profiles and the status
contract. Motion is currently camera-derived, with an explicit experimental limit.

`GraphicsModel` remains independent: its hot-applied presets/custom Metal function
filter the final SDR picture. Turning that color filter Off does not alter the
saved upscaling profile. [Color shader contract](SHADERS.md)

## Installation and recovery

`scripts/install-bridge.py` validates the supplied checkout and app, preserves restore copies, installs the bridge source/integration changes, runs MacNativeGames' runtime updater, and extracts the local editor resources. The existing game modules, keys, firmware, and game data are inputs owned by the player; they are not release contents.

Backups are local. Keep their printed locations. Restoring an application bundle and restoring a gameplay profile are separate operations; replacing the bridge alone does not undo edits the game has already saved.

## Distribution boundary

The public repository contains editor source, bridge code/patches, original interface artwork, catalog names/IDs with attribution, and editor-only screenshots. It contains no Nintendo game programs or assets, firmware, keys, save files, memory dumps, player logs, or generated gameplay images.
