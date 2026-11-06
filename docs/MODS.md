# MacNative Mods

MacNative Mods manages Switch RomFS mods in a separate macOS app. It uses Apple silicon, macOS 26, Python 3.14, and the MacNativeGames checkout at commit [`b1a49a2`](https://github.com/ly05010419/MacNativeGames/tree/b1a49a26650eb31a986e7841f449bb44446c5208). Open the app, select an installed game, and choose **MacNativeGames Folder…** if it cannot find your checkout from a previous setup.

| Game | Versions | Verification |
| --- | --- | --- |
| Breath of the Wild | 1.6.0, 1.9.0 | 1.6.0 used with the installed game. Both versions passed synthetic overlay tests. |
| Tears of the Kingdom | 1.0.0, 1.2.1, 1.4.3 | Synthetic import, manifest, apply, backup and disable tests. |
| Octopath Traveler 0 | 1.0.3, 1.0.4, 1.0.5 | Synthetic import, manifest, apply, backup and disable tests. |

Each game version gets its own library. On first opening BOTW 1.6.0, MacNative Mods copies the library from the former editor tab and preserves its enabled order. BOTW Editor 0.1.4 uses the existing 0.1.3 runtime bridge. MacNative Mods runs without the bridge.

Download [MacNative Mods](https://github.com/christianfurr/botw-editor/releases/tag/mods-v0.1.0) or build it with `bash scripts/build-mod-manager.sh`. The app is ad hoc signed. For a downloaded copy, use the standard [macOS Open Anyway flow](https://support.apple.com/guide/mac-help/open-a-mac-app-from-an-unknown-developer-mh40616/mac) if macOS requests it.

## Import and apply

1. Open **MacNative Mods**, select a game, and choose **Import…**. Use **Add Game…** to select a build outside Applications. Select a folder, ZIP, or 7z containing complete Switch RomFS files.
2. Select an imported pack and turn on its **Enable** switch. Use **Move up** or **Move down** to set its priority. When enabled packs contain the same RomFS path, the later pack supplies the whole file. Distinct paths combine.
3. Quit the selected game. Click **Apply for next launch**, then reopen the game. The manager prepares the overlay while the game is closed, and MacNativeGames reads it at startup.
4. To disable a pack, turn off its switch, apply again with the game closed, and reopen the game. Turning every switch off and applying disables the current overlay for the next launch.

For BOTW 1.6.0, choose **Add demo mods** to generate **Explore Menu** and **Moonlit Apples** from your own installed game. Explore Menu changes the US English title screen's **Continue** label to **Explore!**. Moonlit Apples recolors the Apple icon in the game's Materials pouch. The source contains the generator. The resulting files stay in the private mod library.

## Folder layouts

Use the selected game’s base title ID. The examples use BOTW’s `01007EF00011E000`. A mod can arrive in any of these layouts. You can select a `romfs` folder directly, and a flat folder with recognizable RomFS roots such as `Actor`, `Pack`, or `UI` also works.

| Layout | Example inside the selected folder or archive |
| --- | --- |
| Mod folder | `My Mod/romfs/Actor/…` |
| Emulator mod folder | `contents/01007EF00011E000/My Mod/romfs/Actor/…` |
| Atmosphère or merged Switch export | `atmosphere/contents/01007EF00011E000/romfs/Actor/…` or `01007EF00011E000/romfs/Actor/…` |

The manager accepts complete base-game Switch RomFS files. Raw `.bnp` packages, Wii U and Cemu layouts, separate DLC-title overlays, ExeFS patches, and cheat codes are outside this workflow. An otherwise usable mixed pack applies its RomFS files and reports that the code is skipped. For an export that contains a DLC-title folder, extract it and choose the base-title folder separately. Use a base export whose changes work on their own.

## BCML and UKMM

**BCML merged Switch exports are supported.** In BCML configured for Switch, select your mods, click **Remerge**, then **Export**. Import the resulting ZIP, 7z, or extracted base-title folder in the manager. Export converts BCML's input packages into complete replacement files. [BCML export guide](https://github.com/NiceneNerd/BCML/blob/master/docs/export.md)

UKMM can also merge and deploy Switch RomFS files. In Switch mode, apply your order and deploy the merged output to a local directory. The **Without Name** deployment layout produces the title-ID folder shown above. Import its base-title folder or `romfs` directory. [UKMM deployment manual](https://nicenenerd.github.io/UKMM/setup/deploy.html)

Use BCML or UKMM before importing if you need to combine edits to different records inside one archive or resource. The manager’s priority rule picks one whole file for each shared path.

## Validation and recovery

Import checks the package layout and validates its paths against the installed game. A usable pack replaces at least one existing file and may also add files. Apply revalidates the enabled packs and stages a manifest with the replacement sizes and hashes and the corresponding stock-file hashes. This confirms path and file integrity. A visible in-game check establishes whether a given mod works with that build, including its resource allocation and dependencies. Supply the matching resource size table when a BOTW resource grows. Both demos fit their original resource sizes. [MacNativeGames RomFS loader](https://github.com/ly05010419/MacNativeGames/blob/b1a49a26650eb31a986e7841f449bb44446c5208/docs/mods.md#breath-of-the-wild)

The active overlay is under `~/Library/Application Support/MacNativeGames/BotW/mods/botw160`. Apply keeps the previous folder beside it as `botw160.backup-*`. **Show backup** reveals it in Finder. To restore one, quit BOTW, move the current `botw160` folder aside if present, rename the chosen backup to `botw160`, then reopen the game. If the manager reports a keyboard-specific overlay, set aside `botw160-keyboard` before applying. That overlay can take priority at launch.

The loaders use the selected game and version folder under MacNativeGames Application Support. [Zelda overlay documentation](https://github.com/ly05010419/MacNativeGames/blob/b1a49a26650eb31a986e7841f449bb44446c5208/docs/mods.md) · [Octopath overlay documentation](https://github.com/ly05010419/MacNativeGames/blob/b1a49a26650eb31a986e7841f449bb44446c5208/games/ot0/README.md)
