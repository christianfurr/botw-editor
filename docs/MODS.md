# Mods

Use the **Mods** tab to import and order Switch RomFS mods for the MacNativeGames **Breath of the Wild 1.6.0** app. The editor uses the installed bridge record, the matching MacNativeGames checkout, and Python 3.14 from the port's setup. [Install the bridge](INSTALLATION.md)

## Import and apply

1. Open **Mods** and choose **Import…**. Select a folder, ZIP, or 7z containing complete Switch RomFS files.
2. Select an imported pack and turn on its **Enable** switch. Use **Move up** or **Move down** to set its priority. When enabled packs contain the same RomFS path, the later pack supplies the whole file. Distinct paths combine.
3. Quit BOTW. Click **Apply for next launch**, then reopen the game. The editor prepares the overlay while the game is closed, and MacNativeGames reads it at startup.
4. To disable a pack, turn off its switch, apply again with BOTW closed, and reopen the game. Turning every switch off and applying disables the current overlay for the next launch.

Choose **Add demo mods** to generate **Explore Menu** and **Moonlit Apples** from your own installed game. Explore Menu changes the US English title screen's **Continue** label to **Explore!**. Moonlit Apples recolors the Apple icon in the game's Materials pouch. The source contains the generator. The resulting files stay in the private mod library.

## Folder layouts

Use the base-game Switch title ID `01007EF00011E000`. A mod can arrive in any of these layouts. You can select a `romfs` folder directly, and a flat folder with recognizable RomFS roots such as `Actor`, `Pack`, or `UI` also works.

| Layout | Example inside the selected folder or archive |
| --- | --- |
| Mod folder | `My Mod/romfs/Actor/…` |
| Emulator mod folder | `contents/01007EF00011E000/My Mod/romfs/Actor/…` |
| Atmosphère or merged Switch export | `atmosphere/contents/01007EF00011E000/romfs/Actor/…` or `01007EF00011E000/romfs/Actor/…` |

The editor accepts complete base-game Switch RomFS files. Raw `.bnp` packages, Wii U and Cemu layouts, separate DLC-title overlays, ExeFS patches, and cheat codes are outside this workflow. An otherwise usable mixed pack applies its RomFS files and reports that the code is skipped. For an export that contains a DLC-title folder, extract it and choose the base-title folder separately. Use a base export whose changes work on their own.

## BCML and UKMM

**BCML merged Switch exports are supported.** In BCML configured for Switch, select your mods, click **Remerge**, then **Export**. Import the resulting ZIP, 7z, or extracted base-title folder in the editor. Export converts BCML's input packages into complete replacement files. [BCML export guide](https://github.com/NiceneNerd/BCML/blob/master/docs/export.md)

UKMM can also merge and deploy Switch RomFS files. In Switch mode, apply your order and deploy the merged output to a local directory. The **Without Name** deployment layout produces the title-ID folder shown above. Import its base-title folder or `romfs` directory. [UKMM deployment manual](https://nicenenerd.github.io/UKMM/setup/deploy.html)

Use BCML or UKMM before importing if you need to combine edits to different records inside one archive or resource. The editor's priority rule picks one whole file for each shared path.

## Validation and recovery

Import checks the package layout and validates its paths against the installed game. A usable pack replaces at least one existing file and may also add files. Apply revalidates the enabled packs and stages a manifest with the replacement sizes and hashes and the corresponding stock-file hashes. This confirms path and file integrity. A visible in-game check establishes whether a given mod works with that build, including its resource allocation and dependencies. Supply the matching resource size table when a BOTW resource grows. Both demos fit their original resource sizes. [MacNativeGames RomFS loader](https://github.com/ly05010419/MacNativeGames/blob/b1a49a26650eb31a986e7841f449bb44446c5208/docs/mods.md#breath-of-the-wild)

The active overlay is under `~/Library/Application Support/MacNativeGames/BotW/mods/botw160`. Apply keeps the previous folder beside it as `botw160.backup-*`. **Show backup** reveals it in Finder. To restore one, quit BOTW, move the current `botw160` folder aside if present, rename the chosen backup to `botw160`, then reopen the game. If the editor reports a keyboard-specific overlay, set aside `botw160-keyboard` before applying. That overlay can take priority at launch.

## Other MacNativeGames versions

This editor targets BOTW 1.6.0. MacNativeGames has related loaders that could support future adapters after separate verification.

| Target | Upstream loader and revalidation needed |
| --- | --- |
| **BOTW 1.9.0** | The same upstream manifest tool targets `BotW/mods/botw190`. A future adapter needs the 1.9.0 app and RomFS, its stock hashes and reserved overlays, and independent demo and startup checks. [MacNativeGames BOTW mods](https://github.com/ly05010419/MacNativeGames/blob/b1a49a26650eb31a986e7841f449bb44446c5208/docs/mods.md#breath-of-the-wild) |
| **Tears of the Kingdom 1.4.3** | Upstream targets `TOTK/mods/v143`. A future adapter needs the game's title and version checks, 1.4.3 asset and dictionary handling, and tests for its UI and heap overlay interactions. [MacNativeGames player mods](https://github.com/ly05010419/MacNativeGames/blob/b1a49a26650eb31a986e7841f449bb44446c5208/docs/mods.md) |
| **Octopath Traveler 0** | Upstream has a separate manifest generator and `OT0/mods/<version>` folder, such as `v104` for 1.0.4. An adapter needs that port's prepared RomFS and app detection, its own content validation, and a live launch check for each target version. [MacNativeGames OT0 mods](https://github.com/ly05010419/MacNativeGames/blob/b1a49a26650eb31a986e7841f449bb44446c5208/games/ot0/README.md#mods) |
