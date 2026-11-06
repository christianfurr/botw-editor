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

4. Read the result and preserve the backup locations printed by the installer. It extracts 135 travel destinations, terrain, localized place labels, and item icons from the local game into the editor's private cache.
5. Launch BOTW and load your gameplay session, then open BOTW Editor.

The release app alone cannot edit an unmodified game runtime. Installing the bridge does not require uploading the game, saves, firmware, or keys.

### Check before changing files

Add `--check` to the installer command to validate compatibility and report whether the runtime source hooks are already integrated. This mode does not write files, create backups, extract local resources, or start a build. Source integration and a confirmed runtime installation are reported separately.

The installer preserves unrelated checkout changes. It refuses differing bridge headers that it does not own, and it records ownership hashes under `~/Library/Application Support/BOTWEditor/Installations/`. Backups and runtime build logs remain local under `~/Library/Application Support/BOTWEditor/Backups/`.

**0.1.3 supports fresh installations and upgrades from 0.1.0, 0.1.1, or 0.1.2.** It checks six allowed source files covering the build recipe, input handling, game integration, renderer, and temporal scaler. Pristine files use the full integration patch. Earlier integrations use the matching legacy or 0.1.2 upgrade patch for the hooks they still need. The installer checks the selected sections together before applying them in one atomic Git operation. Conflicting local edits stop installation before source changes.

The installer hashes all three packaged patches and checks them again after creating backups. A changed patch stops the update. Ownership checks cover all nine managed headers. The renderer receives the post-processing and temporal configuration headers, and the runtime receives the editor bridge headers.

When the bridge source and installed runtime already match, the installer reuses verified terrain, label, and item caches. It regenerates missing or invalid caches independently of the runtime build and verifies the outputs before reporting success. Extraction logs stay under `~/Library/Application Support/BOTWEditor/logs/`.

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
| Inventory data could not be validated | Wait for a fresh snapshot and check that BOTW is running with the matching 0.1.3 bridge. |
| Rune selection busy or unavailable | Resume normal gameplay and retry **Unlock runes**. Read the result to see whether the access flags were applied and the selection request is pending. |

## Inventory values

Materials and arrows use stack quantities from 1 to 999. Swords, bows, and shields show **durability points**, with up to two decimal places. The bridge uses 100 raw units per point, so 20 displayed points correspond to a native value of 2,000. The UI converts points to the validated native integer; it requires weapons to be unequipped before editing. Armor, food, and key-item values are not editable. See [Testing](TESTING.md) for the latest durability revision's live status.

The 0.1.1 bridge recognizes the game's armor dye metadata, including its non-dyeable marker. Owning such armor no longer hides the pouch or produces an incorrect instruction to close the game menu.

## World changes

**Enable in-game map** enables the Slate's map screen. Press **Tab** or **M** in BOTW to open it. **Reveal map** enables the 15 tower regions. The editor's map uses a separate local terrain cache.

Activating a shrine discovers it on the in-game map and enables its travel point. Reopen the map to see the updated state. The separate **Mark cleared** and **Mark all cleared** actions update the clear state and increase the counter for newly cleared shrines. These actions cover the shrine's discovery, travel, and clear fields and the counter. **Full Access** discovers and activates base shrine travel points, reveals tower regions, grants base runes and the paraglider, and raises the world towers. It skips the opening tower activation sequence.

**Unlock runes** grants the base kit and enables Slate access. When the game has no selection, the bridge requests an initial Bomb and lets the game apply it. During ordinary unmounted gameplay with default keyboard bindings, hold **3** to choose a rune, release it, then press **F** to use it.

The editor shows game terrain and zoom-dependent place labels alongside 120 base shrine and 15 tower destinations. From an outdoor `MainField` session, choose a destination marker or list entry to travel.

## Local terrain, place labels, and item artwork

The editor reads local game data from `~/Library/Application Support/BOTWEditor/cache/`. The `map/` folder holds the terrain image, projection metadata, and `labels.json`. The `items/` folder holds item icons and their resolution manifest. Keep these generated files in your private cache and out of the release bundle.

To regenerate missing artwork or labels, run the corresponding command from the matching source checkout.

```sh
python3 scripts/extract_map.py --app "/Applications/Breath of the Wild.app"
python3 scripts/extract_map_labels.py --app "/Applications/Breath of the Wild.app"
python3 scripts/extract_items.py --app "/Applications/Breath of the Wild.app"
```

Restart the editor after extraction. A missing icon uses a system-symbol fallback; catalog names alone do not guarantee that an item can be granted by your game version. The native game still applies inventory capacity, duplicate-item, and DLC rules. [Extraction details](MAP-EXTRACTION.md)

## Restore

Quit BOTW and BOTW Editor before restoring files. Use the application/profile copies created before the change and the paths printed by the installer.

- To remove the bridge, restore the backed-up **game app**. This does not revert saved inventory or world changes.
- To undo saved gameplay changes, restore the matching **profile** backup. Move the current profile aside first, then restore the complete backed-up profile rather than merging old and new files.
- Keep the displaced app/profile until the restored installation starts and the intended save loads.

Profiles are stored under `~/Library/Application Support/MacNativeGames/BotW/profile/`. A runtime update, an editor update, and a profile restore have different effects; choose the restore copy for the change you intend to undo.
