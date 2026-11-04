# Local terrain map

`scripts/extract_map.py` reads the player's installed BOTW RomFS and produces the
actual Hyrule terrain artwork. It does not download resources, modify the game,
open a gameplay session, or write to a player profile. No Nintendo artwork is
included in this repository or the editor release.

Run with Python 3.10 or newer; no extra packages are required:

```sh
python3 scripts/extract_map.py --app '/Applications/Breath of the Wild.app'
```

The default output is private:

```text
~/Library/Application Support/BOTWEditor/cache/map/map.png
~/Library/Application Support/BOTWEditor/cache/map/map.json
```

The PNG contains the terrain. The JSON records its dimensions, SHA-256 checksum,
source detail level, grid, world bounds, and world-to-pixel transform. Generated
files have mode `0600`. Keep them outside the public repository and release ZIP.
Missing or unsupported textures cause an error instead of fabricated terrain.

The default detail level uses the game's 840×840-pixel `MapTex1` tiles, yielding a
10080×8400 image. `--level 2` uses 300-pixel tiles for a 3600×3000 image;
`--level 3` uses 135-pixel tiles for a 1620×1350 overview. `--romfs DIRECTORY` can read an existing extracted
RomFS folder with `index.json` or `runtime-index.bin` and `romfs.bin`.

## Coordinates

The full north-up grid has columns `Z,A,B,C,D,E,F,G,H,I,J,K` and rows `0…9`.
Each tile covers 1000 world units. The image spans X `[-6000,6000]` and Z
`[-5000,5000]`; decreasing Z is north. These bounds include the map's outer
terrain margin. Pixel coordinates start at the upper-left edge:

```text
pixel_x = (world_x + 6000) / 12000 * image_width
pixel_y = (world_z + 5000) / 10000 * image_height
```

Use the full image rectangle to project destination and player coordinates.
Letterbox when its aspect ratio differs from the view. Do not recompute the world
bounds from search results or crop tile edges.

## Format and validation

The extractor reads 120 unsuffixed `UI/MapTex/MainField/MapTexN_*.sbmaptex` files.
They contain Yaz0-compressed BFRES resources with a single BNTX texture. BC1 sRGB
blocks use Tegra block-linear addressing. The script checks texture dimensions,
format, mip count, data extents, complete grid coverage, and a 100-megapixel output
limit before decoding and stitching the base tiles. Region-mask variants with
additional suffixes are excluded.

The BC1 output was independently compared byte-for-byte with Pillow's DDS decoder.
Installed binary-index and extracted JSON-index paths use the same RomFS readers
as destination extraction. Native NX160 map initialization also stores the full
12000×10000 extents; the inner playable tile grid is 10000×8000.

Primary coordinate references:

- [ZeldaMods objmap world extents](https://github.com/zeldamods/objmap/blob/master/src/util/map.ts)
- [ZeldaMods objmap tile grid](https://github.com/zeldamods/objmap/blob/master/src/components/AppMap.ts)

The Tegra GOB formula is adapted from MacNativeGames' MIT-licensed
`make_button_icons.py`; see `THIRD_PARTY_NOTICES.md`. Texture field/layout facts
were checked against the local game's BNTX headers. The extractor contains format
handling code and resource paths, never extracted game bytes.

## Item icons

`scripts/extract_items.py` uses the same local RomFS readers to decode the player's inventory icon textures. It writes PNG files and `manifest.json` under `~/Library/Application Support/BOTWEditor/cache/items/`. Game actor metadata resolves shared icons for upgraded or variant items; the manifest records direct, metadata-based, and catalog-only legacy resolutions separately.

```sh
python3 scripts/extract_items.py --app "/Applications/Breath of the Wild.app"
```

An icon match is a display mapping, not a guarantee that the game accepts that catalog actor as an inventory item. Missing icons use the editor's system-symbol fallback. Neither these generated icons nor the terrain image may be copied into the public app bundle, repository, screenshots, or release archive.
