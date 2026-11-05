# BOTW Editor

A native Mac editor for your running **Breath of the Wild 1.6.0** session in [MacNativeGames](https://github.com/ly05010419/MacNativeGames).

Browse your pouch, give items, change player stats, and travel around Hyrule from a separate window. The Sheikah-inspired interface uses compact navigation, an item grid with a persistent inspector, and travel controls over the actual local map.

**0.1.2 is an early prerelease for one specific game/runtime version.** See the [live testing record](docs/TESTING.md) for verified operations and remaining checks.

[Download 0.1.2](https://github.com/christianfurr/botw-editor/releases/tag/v0.1.2) · [Source and setup](docs/INSTALLATION.md)

![BOTW Editor player controls](Assets/screenshots/main.png)

## What you can edit

| Area | Controls |
| --- | --- |
| Inventory | Browse a pouch grid and search a 1,012-item catalog. Add items, change stack quantities or unequipped weapon durability, and remove entries. |
| Player | Rupees, maximum hearts, maximum stamina, and optional current-health/stamina locks. |
| World | Activate a selected shrine or all 120 base-game shrine travel points; reveal the 15 tower map regions; acquire base runes and the paraglider. Activation does not clear a shrine. |
| Full Access | Apply those travel unlocks and raise the world towers. **This skips the opening tower activation sequence.** It does not directly mark shrines or story quests complete. |
| Map | Browse the game's Hyrule terrain map, search 120 base shrines and 15 towers, and select a destination for travel. A live player marker shows your position. |
| Upscaling | Experimental MetalFX Temporal reconstruction with Quality/Balanced presets for 1440p or 4K output. Preset changes require restarting BOTW. |
| Color & custom shaders | Use Off, Natural, Vivid, or Cinematic picture filters, adjust strength and sharpness, or write a custom Metal color shader. |

The installer builds the terrain map and item icons **locally from your own game**. They stay in your private Application Support cache and are not included in this repository or the editor download. [Local artwork and map extraction](docs/MAP-EXTRACTION.md)

Version 0.1.1 fixes an inventory scan that treated undyed armor as an invalid entry and incorrectly told players to close the game menu. The catalog now uses the same icon-grid layout as the pouch, keeps quantity and **Give item** visible, and displays the actual inventory blocker when one exists. Browsing remains available while the game is closed.

Controls wait for a supported gameplay session and fresh runtime state. The editor displays the game's acknowledgement and readback for each request. During travel, it disables edits while the game loads; an accepted request does not confirm arrival.

The 0.1.0 live checks covered item grants and removal, an unequipped weapon durability round trip, rupees, 30 hearts, three stamina wheels, world-access flags, save/reload persistence, and three travel destinations. The 0.1.1 runtime also passed grants while the system and Materials menus were open, the undyed-armor case, and visible custom-shader/error-recovery checks. The 0.1.2 candidate has also completed real MetalFX temporal frames at 1440p and 4K, including a 4K camera/walk and menu-resume check. The [testing record](docs/TESTING.md) separates candidate execution, final-rebuild replay and their visual-quality limits.

## MetalFX Temporal

![MetalFX Temporal status at 4K](Assets/screenshots/upscaling.png)

Open the sliders button, choose **Upscaling**, select a preset, then click **Use on next launch**. Quit and reopen BOTW. The panel reports the actual render/output dimensions and completed GPU work; saving a setting alone does not mean it is active.

Quality at 4K renders at **2560 × 1440** and reconstructs **3840 × 2160**. Quality at 1440p renders at **1920 × 1080** and reconstructs **2560 × 1440**. Balanced presets use a lower input resolution. Off retains the port's original scaling.

This integration currently estimates camera motion and uses a depth-reactive mask. Moving objects and newly revealed regions can show artifacts. It is experimental: no DLSS-equivalent quality or measured frame-rate gain is claimed. Color filters remain a separate setting. [Presets, status and limitations](docs/TEMPORAL.md)

## Requirements

- Apple silicon Mac running **macOS 26 or later**.
- An existing, working MacNativeGames **BOTW 1.6.0** app built from your own game files.
- MacNativeGames checkout at supported commit [`b1a49a2`](https://github.com/ly05010419/MacNativeGames/tree/b1a49a26650eb31a986e7841f449bb44446c5208), with its build environment and work folder available.
- The BOTW Editor runtime bridge installed into that app.

BOTW 1.9.0, Tears of the Kingdom, physical Switch consoles, and standard Switch cheat files are outside this release's compatibility target.

## Install

1. Download **BOTW.Editor.zip** and **Source code (zip)** from the same repository release. Unzip both and move **BOTW Editor.app** to Applications. Open a terminal in the unpacked source folder for the bridge installer.
2. Quit BOTW, then install the bridge from this repository:

   ```sh
   python3 scripts/install-bridge.py \
     --repo "/path/to/MacNativeGames" \
     --app "/Applications/Breath of the Wild.app" \
     --work "/path/to/BotW_build"
   ```

3. Keep the backup paths printed by the installer. It checks compatibility, preserves restore copies, rebuilds the runtime, and extracts your local destinations, terrain map, and item icons.
4. Open BOTW and load your game. Open **BOTW Editor** and wait for its connected state.

The editor release contains no game files, firmware, keys, or saves. The bridge uses the local files already present in your MacNativeGames installation.

The current app is **ad hoc signed and not Apple-notarized**. If macOS blocks the downloaded app after you try to open it, use **System Settings → Privacy & Security → Open Anyway** for BOTW Editor and confirm the app name. You can also build it locally from source. [Apple's opening instructions](https://support.apple.com/en-us/102445)

## Use

- **Inventory:** select a tile to inspect it. Apply a new stack quantity or durability value in the detail pane, or choose **Remove Item…**. Unequip gear before editing its durability or removing it. Use **Give item…** to browse the catalog, choose a quantity, and select **Give item**. A successful grant stays visible in the result bar. If Link is holding cooking materials, put them away before editing inventory.
- **Player:** use the stat buttons to apply values. Maximum hearts and stamina wait for the live player values to load. The health lock holds your current health; the stamina lock refills stamina. Both locks start off.
- **World:** select a map marker or destination row for its travel and shrine actions. The **World access** menu contains bulk unlocks. Read the confirmation before applying **Full access**, which also skips the opening tower activation sequence.
- **Upscaling:** open the sliders button and choose **Upscaling**. Save a preset with **Use on next launch**, then fully quit and reopen BOTW. The saved preset and the current session's active status are displayed separately.
- **Color & custom shaders:** choose this section of the same panel to adjust the final picture. Its **Off** restores unfiltered color without changing the upscaling preset. Custom source and fallback behavior are documented in [Shaders](docs/SHADERS.md).
- **Tower travel:** activate the first tower in BOTW or use **Full Access** before traveling to a tower that the editor marks unavailable.
- Read the action result before making another edit. The window disables edits when the game closes, a bridge update is required, or connection data becomes stale.

Changes can become permanent when the game saves. Keep a profile backup before changing inventory or progress. See [Installation and recovery](docs/INSTALLATION.md).

## Build from source

Use an Apple silicon Mac on macOS 26 or later, with Xcode command-line tools that provide a macOS 26 or newer SDK. Run:

```sh
./scripts/build.sh
```

The build runs the Swift model/UI/graphics checks, native runtime and temporal-input guards, real MetalFX probes, synthetic color-filter pixel checks, and installer fixtures. It produces `build/BOTW Editor.app`, `build/BOTW Editor.zip`, and a SHA-256 checksum alongside the ZIP. The app includes the item-name catalog, original interface artwork, native icon, and license notices. Building the editor needs no game files.

To run the checks without packaging the app:

```sh
./scripts/test.sh
```

The macOS 26 CI job runs the same editor build without installing the bridge or launching a game. A runner without a Metal device reports a GPU-test skip; that skip does not prove GPU results. Builds use ad-hoc code signing; they are not notarized or signed with a Developer ID certificate.

Runtime bridge installation is a separate step. Rebuilding the editor alone does not update the game app.

## Project notes

- [Architecture](docs/ARCHITECTURE.md)
- [Testing and release evidence](docs/TESTING.md)
- [Installation and recovery](docs/INSTALLATION.md)
- [Private terrain map and item artwork](docs/MAP-EXTRACTION.md)
- [MetalFX Temporal reconstruction](docs/TEMPORAL.md)
- [Color filters and custom Metal shaders](docs/SHADERS.md)
- [Third-party notices](THIRD_PARTY_NOTICES.md)

MIT licensed. This independent project is not affiliated with Nintendo. The Legend of Zelda and Nintendo Switch are trademarks of Nintendo.
