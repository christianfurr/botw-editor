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
- Shrine activation sets both `Open_Dungeon###` and the S32 `Location_Dungeon###` discovery value. Full Access performs 266 typed edits: 146 booleans and 120 discovery integers, including `MapTower_07_Demo`, which raises the world towers and skips the opening tower activation sequence. Full Access leaves shrine-clear and story-quest completion fields alone.
- Travel uses the game's native reset route from an outdoor `MainField` session. The bridge rejects unsupported interior contexts and unavailable tower destinations. See the live verification record for outcome-specific limits.

## Master Sword, capacity and map access

The usable Master Sword is `Weapon_Sword_070`. Internal actors `056`, `071` and
`072` are world/appearance forms and are refused as pouch grants. The editor
offers the canonical sword instead and reports an explicit duplicate refusal.
Awakening edits require exactly one owned, unequipped canonical sword and a
ready inventory; selecting a level sends nothing until **Apply level**.

The level transaction changes only the native saved fields below in both
primary and pending GameData stores, with preflight, readback and rollback:

| Level | Normal attack | `MasterSword_Add_Power` | `Open_MasterSword_FullPower` |
| --- | --- | --- | --- |
| 0 | 30 | 0 | false |
| 1 | 40 | 10 | false |
| 2 | 50 | 20 | false |
| 3 | 60 | 30 | true |

Beam power, current durability, recharge state and quest flags are separate.
The game's situational awakening can still raise attack to 60. A configured
normal-attack value does not promise the currently displayed combat value.

Pouch-capacity edits use saved S32 fields `WeaponPorchStockNum`,
`BowPorchStockNum` and `ShieldPorchStockNum`. Bounds are 8–20, 5–14 and 4–20.
Each category has its own Apply action; decreases relative to either game
store refuse. The transaction verifies both stores and rolls back a failed edit.
It does not grant items, spend Korok seeds or change Hestu progress.

In-game map access changes exactly `PortableUnit_PowerOn` and
`IsGet_PortableUnit`, the two repeatable saved booleans enabled by NX160's Slate
pickup event. The existing typed flag transaction handles both stores. It does
not grant a key item, open the first door, reveal tower regions or complete a
quest. The editor's local terrain map and the `reveal_map` action are separate.
After enabling access, the player focuses BOTW and presses Tab or M.

The protocol adds optional `master_sword`, `pouch_slots`, `map_access`,
`shrine_completion` and `horse_speed` capabilities. Readbacks include sword
level/normal attack/full power, capacities, map access, shrine clear state and
current horse-speed availability. Missing fields preserve older-runtime
decoding; the corresponding controls require a bridge update.

## Shrine discovery, clearing and runes

Activation verifies both GameData stores, sets Open true and raises discovery
to at least 1 while preserving each store's existing positive value. A shrine's
`unlocked` readback requires both Open and discovery greater than zero. The
exact NX160 `Location_Dungeon001` flag is readable/saveable but not normally
program-writable. Only that name, S32 type and property byte `0x05` can use the
native setter's permission bypass, after its forwarding and property-check
instructions match. Other discovery writes retain normal native checks.

Completion is a separate explicit command. It activates/discovers targets,
sets their `Clear_Dungeon###` booleans, and increments `DungeonClearCounter`
only for false-to-true Clear transitions. Both stores' clear states and counter
must agree; mismatches and counter overflow refuse. Clearing all 120 base
shrines uses 361 typed edits within a 384-edit transaction bound. Preflight,
final readback and rollback cover the whole batch. The counter's exact native
read-only metadata has its own narrowly scoped setter adapter. No Spirit Orb,
Enter, chest, quest or other reward fields are written. Optional `completed`
destination state remains separate from `unlocked`.

Rune acquisition sets seven base-rune acquisition booleans and the same two
Slate-access booleans as the pickup event. `botw_editor_runes.h` also handles
the separate selection step found in NX160's native download UI. Before the
flag edit, it checks the manager's guarded identity and nonblocking mutex
availability. After ownership succeeds, it requests stock Bomb ID 0 only if
both the requested and currently applied selection are -1. Existing valid
selections remain unchanged.

Only the requested-selection field is written under the native TryLockMutex;
the game's normal update applies it, updates the current/previous fields and
persists `PlayerEquipItemType`. The helper does not call the manager's broad
reset routine or write applied state directly. A successful request is not an
applied-selection claim. Optional `rune_selection_requested`,
`rune_selection_current`, `rune_selection_previous` and `rune_selection_saved`
diagnostics expose the difference; unavailable readbacks are null.

The selection phase can become busy after ownership flags were committed. Its
result must distinguish enabled access from an unconfirmed selection and ask
for a retry. On write/readback failure, rollback restores only a request still
proven to be this helper's own write; unknown competing changes are preserved.
The default keyboard instructions are hold 3 to select, release, then F to
use. The game's normal unmounted gameplay context is required for testing.

## Session horse movement

`botw_editor_horse.h` resolves the game's process-owned AnimalUnitSpeed resource
through its guarded native getter. It verifies resource identity, VM access,
the full original-table fingerprint, ordered keys and numeric bounds before
capturing a baseline. Exactly 104 speed cells cover four ordinary land gears
for 26 standard horse types. Adjacent animation values, special mounts and
unrelated animals remain byte-identical.

Each 1.00–2.00× selection scales from the original baseline. Each write checks
identity and ownership; complete readback confirms success. Failed writes roll
back owned values. Unexpected external changes refuse further edits rather
than overwriting another owner. Off restores the original speeds; a replaced
resource requires a new verified baseline. The change affects this session's
shared table, not only the mounted horse, and edits no save values.

## Local map, destinations, and item icons

The installer extracts 120 base-shrine and 15 tower destinations from the player's game. It also decodes the player's terrain tiles, localized map labels and item icons into `~/Library/Application Support/BOTWEditor/cache/`. No extracted artwork or place-label data enters the source tree or app bundle.

The companion projects those 135 destination markers and the current player position over the local terrain image, with north at the top. Negative world Z maps toward the top of the canvas. Terrain bounds remain fixed when search filters change, and the view letterboxes the map rather than stretching its projection. Selecting a marker or row chooses a known destination; the UI does not provide arbitrary click-to-coordinate travel. [Format and coordinate details](MAP-EXTRACTION.md)

`extract_map_labels.py` uses the game's original label positions and localized
location names. The verified extraction contains 557 labels: 8 regions, 24
areas, 34 settlements and 491 landmarks. `labels.json` stays in the private map
cache; validation covers its digest, bounded coordinates and label kinds.
Zoom thresholds reduce clutter by showing broad regions first and smaller
places at closer views. Labels do not add teleport destinations.

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

Nine managed headers and a six-source-file fresh-install patch carry the
integration. Separate legacy-to-0.1.3 and 0.1.2-to-0.1.3 patches add only the
missing allowed hooks on supported older installations. Installer ownership
and conflict checks apply to all managed headers and source changes.

Backups are local. Keep their printed locations. Restoring an application bundle and restoring a gameplay profile are separate operations; replacing the bridge alone does not undo edits the game has already saved.

## Distribution boundary

The public repository contains editor source, bridge code/patches, original interface artwork, catalog names/IDs with attribution, and editor-only screenshots. It contains no Nintendo game programs or assets, firmware, keys, save files, memory dumps, player logs, or generated gameplay images.
