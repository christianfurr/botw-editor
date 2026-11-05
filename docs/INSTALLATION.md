# Installation and recovery

## Install the editor and bridge

BOTW Editor requires a working MacNativeGames BOTW **1.6.0** app and the matching MacNativeGames checkout/build environment. The initial bridge supports commit `b1a49a2`.

1. Quit BOTW.
2. Move the release's `BOTW Editor.app` to Applications.
3. From the matching BOTW Editor source checkout, run:

   ```sh
   python3 scripts/install-bridge.py \
     --repo "/path/to/MacNativeGames" \
     --app "/Applications/Breath of the Wild.app" \
     --work "/path/to/BotW_build"
   ```

4. Read the result and preserve the backup locations printed by the installer. It also extracts the 135 destinations, terrain map, and item icons from the local game into the editor's private cache.
5. Launch BOTW and load your gameplay session, then open BOTW Editor.

The release app alone cannot edit an unmodified game runtime. Installing the bridge does not require uploading the game, saves, firmware, or keys.

### Check before changing files

Add `--check` to the installer command to validate compatibility and report whether the runtime source hooks are already integrated. This mode does not write files, create backups, extract local resources, or start a build. Source integration and a confirmed runtime installation are reported separately.

The installer preserves unrelated checkout changes. It refuses differing bridge headers that it does not own, and it records ownership hashes under `~/Library/Application Support/BOTWEditor/Installations/`. Backups and runtime build logs remain local under `~/Library/Application Support/BOTWEditor/Backups/`.

**0.1.2 supports fresh installations and upgrades from 0.1.0 or 0.1.1.** It checks six allowed source files: the build recipe, input handling, game patch integration, renderer, and temporal-scaler implementation/header. Existing hooks are retained. Pristine files use the full integration patch; the two files changed again since 0.1.1 use a separately validated compatibility patch. The installer checks and applies the selected sections together in one atomic Git operation. Conflicting local edits stop installation before source changes.

Both packaged patches are included in installation ownership hashes and checked again after backups, so changing either patch during installation stops the update. The seven managed headers retain their recorded ownership checks. Post-processing and temporal-configuration headers are installed alongside the renderer; editor bridge headers stay with the runtime.

When the bridge source and installed runtime already match, it reuses hash-verified artwork caches. Missing or invalid local artwork is regenerated without rebuilding the game runtime; extraction logs are kept under `~/Library/Application Support/BOTWEditor/logs/`. The installer verifies the extracted outputs before reporting success.

### Opening the downloaded app

The current release is ad hoc signed; it is not signed with a Developer ID or notarized by Apple. After attempting to open it, macOS may require you to open **System Settings → Privacy & Security**, choose **Open Anyway** for BOTW Editor, and confirm the app name. The system may ask you to authenticate. Alternatively, build the app locally from source. [Apple documents this process](https://support.apple.com/en-us/102445).

No step requires disabling Gatekeeper or removing quarantine attributes.

## Connection problems

| State | What to check |
| --- | --- |
| Game closed | Open the supported BOTW app. |
| Waiting for gameplay | Load your game; the title/loading screen may not expose the required live objects. |
| Maximum-stat buttons unavailable | Wait for the live player values to load. These edits need the current player cache as well as GameData. |
| Teleport loading | Wait for the game to finish its native loading/reset sequence. Acceptance alone does not confirm arrival. |
| Tower destination unavailable | Activate the first tower in BOTW or choose Full Access. Full Access raises the towers and skips the opening tower activation sequence. |
| Stale or missing bridge state | Verify that the bridge installer finished successfully and that you launched the updated app. |
| Feature unavailable | That operation failed its compatibility/readiness checks. Do not substitute offsets from another game version. |
| Request rejected | Read the editor's result; a queued command or button click is not proof of an applied edit. |
| Inventory edits unavailable while holding materials | Put away the materials Link is holding, then retry. Browsing the catalog remains available. |
| Inventory data could not be validated | Wait for a fresh snapshot and verify the 0.1.1 bridge is installed. An open game menu alone does not require closing it. |

## Inventory values

Materials and arrows use stack quantities from 1 to 999. Swords, bows, and shields show **durability points**, with up to two decimal places. The bridge uses 100 raw units per point, so 20 displayed points correspond to a native value of 2,000. The UI converts points to the validated native integer; it requires weapons to be unequipped before editing. Armor, food, and key-item values are not editable. See [Testing](TESTING.md) for the latest durability revision's live status.

The 0.1.1 bridge recognizes the game's armor dye metadata, including its non-dyeable marker. Owning such armor no longer hides the pouch or produces an incorrect instruction to close the game menu.

## World changes

Shrine activation enables its travel point; it does not clear the shrine. **Reveal Map** enables the 15 tower map regions. **Full Access** adds the base shrine travel points, map regions, runes, and paraglider, then raises the world towers. It skips the opening tower activation sequence without directly marking shrine clears or story quests complete.

The map combines the game's terrain artwork with 135 destinations: 120 base shrines and 15 towers. Choose a marker or list entry to travel; selecting arbitrary terrain does not request a coordinate warp. Travel targets outdoor `MainField` locations, and the bridge rejects unsupported interiors.

## Local map and item artwork

The editor reads locally extracted artwork from `~/Library/Application Support/BOTWEditor/cache/`. The `map/` folder holds the terrain image and projection metadata; `items/` holds item icons and their resolution manifest. These are private copies made from your installed game. They are not stored in the editor bundle or uploaded.

If artwork is missing, regenerate it from the same source checkout without rebuilding the editor:

```sh
python3 scripts/extract_map.py --app "/Applications/Breath of the Wild.app"
python3 scripts/extract_items.py --app "/Applications/Breath of the Wild.app"
```

Restart the editor after extraction. A missing icon uses a system-symbol fallback; catalog names alone do not guarantee that an item can be granted by your game version. The native game still applies inventory capacity, duplicate-item, and DLC rules. [Extraction details](MAP-EXTRACTION.md)

## Restore

Quit BOTW and BOTW Editor before restoring files. Use the application/profile copies created before the change and the paths printed by the installer.

- To remove the bridge, restore the backed-up **game app**. This does not revert saved inventory or world changes.
- To undo saved gameplay changes, restore the matching **profile** backup. Move the current profile aside first, then restore the complete backed-up profile rather than merging old and new files.
- Keep the displaced app/profile until the restored installation starts and the intended save loads.

Profiles are stored under `~/Library/Application Support/MacNativeGames/BotW/profile/`. A runtime update, an editor update, and a profile restore have different effects; choose the restore copy for the change you intend to undo.
