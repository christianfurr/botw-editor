# Mods review

The import review reproduced two archive entries resolving to one path. The importer now checks the extraction listing first for aliases, including case and Unicode equivalents. Composition refuses those aliases across packs and accepts exact file overlaps in the chosen order. Selecting `romfs/` inspects that data tree. The importer omits ExeFS files and reports skipped code. An empty library leaves an unmanaged overlay in place.

An isolated live launch exposed directory timestamp changes that made an applied set look stale. The prepared signature now uses the manifest and file identity, size, and modification time. The game can load the prepared set without changing that status. Apply refuses while the game is open. Disabling the set keeps the previous files in a backup and restores the stock title on the next launch.

The local build and Swift typecheck passed. The AppKit panel rendered at 1000 × 680 and 1440 × 820. The final CLI accepted a BCML-layout 7z and Switch and emulator ZIP layouts. The game verified two files, displayed Explore!, and showed the blue Apple icon. The kept profile matched all 23 saved file hashes.

Raw BNP merge logs, Wii U assets, DLC-title overlays, and ExeFS patches remain outside the importer. The demo run verifies the supported file path and two mods; it does not establish compatibility for every third-party pack. See [the testing record](../../../docs/TESTING.md) and [the Mods guide](../../../docs/MODS.md).
