#!/usr/bin/env python3
"""Extract private BOTW map text and its original world positions.

Reads only the player's installed game. Nintendo names stay in the private
Application Support cache, never in the repository or editor bundle.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import struct
from pathlib import Path

from extract_destinations import byml, clean, member, msbt_labels, read_index, sources, unyaz

DEFAULT_OUTPUT = Path.home() / "Library/Application Support/BOTWEditor/cache/map"
WORLD = {"min_x": -6000.0, "max_x": 6000.0, "min_z": -5000.0, "max_z": 5000.0}


def label_digest(labels: list[dict]) -> str:
    return hashlib.sha256(json.dumps(labels, sort_keys=True, separators=(",", ":"),
                                     ensure_ascii=False).encode("utf-8")).hexdigest()


def make_labels(locations: list, markers: list, names: dict) -> list[dict]:
    """Keep each name's wider-view placement, then add named settlements.

    Location.smubin is the game's text-placement layer, not a centroid inferred
    from terrain or a warp destination. Repeated close-view river/lake labels
    collapse to their original wider-view placement when one exists.
    """
    if not isinstance(locations, list) or not isinstance(markers, list):
        raise ValueError("Missing original map label tables")
    if any(not isinstance(entry, dict) for entry in locations + markers):
        raise ValueError("Invalid original map label entry")
    chosen = {}
    settlement_icons = {"Village", "CheckPoint", "Labo", "Castle", "StartPoint", "Hatago"}
    settlement_ids = {entry.get("MessageID") for entry in markers if entry.get("Icon") in settlement_icons}
    candidates = [(entry, False) for entry in locations]
    candidates += [(entry, True) for entry in markers if entry.get("Icon") in settlement_icons]
    for entry, marker in candidates:
        identifier = entry.get("MessageID")
        if not isinstance(identifier, str) or not identifier or identifier not in names:
            continue
        text = clean(names[identifier])
        if not text or len(text) > 96 or not all(character.isprintable() for character in text):
            raise ValueError("Invalid localized map label")
        position = entry.get("Translate", {})
        try:
            if type(position["X"]) not in (int, float) or type(position["Z"]) not in (int, float):
                raise ValueError("Non-numeric map coordinate")
            x, z = float(position["X"]), float(position["Z"])
        except (KeyError, TypeError, ValueError):
            raise ValueError("Invalid original map label position") from None
        if not math.isfinite(x) or not math.isfinite(z) or not -6000 <= x <= 6000 or not -5000 <= z <= 5000:
            raise ValueError("Map label position is outside Hyrule")
        level = entry.get("ShowLevel", 4)
        if type(level) is not int or level not in (1, 2, 4, 8):
            raise ValueError("Unexpected map text visibility level")
        kind = "region" if identifier.startswith("MapRegion_") else "area" if level <= 2 else "settlement" if identifier in settlement_ids else "landmark"
        label = {"id": identifier.strip(), "name": text, "x": x, "z": z,
                 "level": level, "kind": kind}
        preference = (marker, level)
        previous = chosen.get(label["id"])
        if previous is None or preference < previous[0]:
            chosen[label["id"]] = (preference, label)
    labels = [entry[1] for entry in chosen.values()]
    if len(labels) > 1000:
        raise ValueError("Unbounded map label collection")
    order = {"region": 0, "area": 1, "settlement": 2, "landmark": 3}
    return sorted(labels, key=lambda label: (order[label["kind"]], label["level"], label["id"]))


def extract(args) -> dict:
    index_path, image_path = sources(args)
    files = read_index(index_path)
    with image_path.open("rb") as image:
        image_size = os.fstat(image.fileno()).st_size

        def read_file(name):
            offset, size = files[name]
            if offset < 0 or size < 1 or size > 256 * 1024 * 1024 or offset + size > image_size:
                raise ValueError("Invalid local RomFS extent")
            image.seek(offset)
            data = image.read(size)
            if len(data) != size:
                raise ValueError("Truncated local RomFS member")
            return data

        boot = read_file("Pack/Bootup.pack")
        locations = byml(unyaz(member(boot, "Map/MainField/Location.smubin")))
        static = byml(unyaz(member(boot, "Map/MainField/Static.smubin")))
        messages = unyaz(member(read_file("Pack/Bootup_" + args.lang + ".pack"),
                               "Message/Msg_" + args.lang + ".product.ssarc"))
        names = msbt_labels(member(messages, "StaticMsg/LocationMarker.msbt"))
    labels = make_labels(locations + static["LocationPointer"], static["LocationMarker"], names)
    if sum(label["kind"] == "region" for label in labels) != 8:
        raise ValueError("Expected the game's eight original regional labels")
    manifest = {"schema": 1, "world_bounds": WORLD, "labels": labels,
                "labels_sha256": label_digest(labels),
                "source": {"game": "BOTW", "platform": "Switch", "language": args.lang,
                           "local_game_assets_only": True,
                           "positions": ["Map/MainField/Location.smubin", "Map/MainField/Static.smubin"]}}
    output = args.output_dir.expanduser().resolve()
    if output.is_relative_to(Path(__file__).resolve().parents[1]):
        raise ValueError("Extracted game labels must stay outside the editor repository")
    output.mkdir(parents=True, exist_ok=True, mode=0o700)
    target = output / "labels.json"
    if target.is_symlink():
        raise ValueError("Refusing a symlinked label cache")
    temporary = target.with_name(target.name + f".{os.getpid()}.tmp")
    try:
        with temporary.open("x", encoding="utf-8") as stream:
            os.chmod(temporary, 0o600)
            json.dump(manifest, stream, indent=2, ensure_ascii=False)
            stream.write("\n")
        temporary.replace(target)
    finally:
        temporary.unlink(missing_ok=True)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--app", type=Path, default=Path("/Applications/Breath of the Wild.app"))
    parser.add_argument("--romfs", type=Path, help="Use an extracted RomFS folder instead")
    parser.add_argument("--lang", default="USen")
    parser.add_argument("--output-dir", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    if not args.lang.isalnum() or len(args.lang) != 4:
        parser.error("Use a four-character game language code")
    try:
        manifest = extract(args)
    except (OSError, ValueError, KeyError, IndexError, struct.error, AssertionError) as error:
        parser.exit(1, f"Cannot extract private map labels: {error}\n")
    print(f"Wrote {len(manifest['labels'])} original-position private map labels to {args.output_dir.expanduser()}")


if __name__ == "__main__":
    main()
