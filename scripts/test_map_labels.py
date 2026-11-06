"""Synthetic map-label extraction checks; no game files or gameplay."""
import math
import unittest

from extract_map_labels import make_labels


def location(identifier, x=100, z=-200, level=4):
    return {"MessageID": identifier, "ShowLevel": level, "Translate": {"X": x, "Y": 0, "Z": z}}


class MapLabelTests(unittest.TestCase):
    def test_original_world_position_and_localized_name_are_preserved(self):
        labels = make_labels([location("MapRegion_Fixture", 123.5, -456.25, 1)], [], {"MapRegion_Fixture": "Fixture Region"})
        self.assertEqual(labels, [{"id": "MapRegion_Fixture", "name": "Fixture Region", "x": 123.5, "z": -456.25,
                                  "level": 1, "kind": "region"}])

    def test_wider_view_placement_wins_without_averaging(self):
        labels = make_labels([location("Lake", 10, 20, 8), location("Lake", 30, 40, 2),
                              location("Lake", 50, 60, 4)], [], {"Lake": "Fixture Lake"})
        self.assertEqual(len(labels), 1)
        self.assertEqual((labels[0]["x"], labels[0]["z"], labels[0]["level"], labels[0]["kind"]), (30, 40, 2, "area"))

    def test_text_layer_wins_over_a_duplicate_marker(self):
        marker = {**location("Town", -10, -20), "Icon": "Village"}
        labels = make_labels([location("Town", 100, 200)], [marker], {"Town": "Fixture Town"})
        self.assertEqual((labels[0]["x"], labels[0]["z"]), (100, 200))

    def test_named_settlements_are_added_but_warp_symbols_are_not_duplicated(self):
        markers = [{**location("Village"), "Icon": "Village"}, {**location("Shrine"), "Icon": "Dungeon"}]
        labels = make_labels([], markers, {"Village": "Fixture Village", "Shrine": "Fixture Shrine"})
        self.assertEqual([(entry["id"], entry["kind"]) for entry in labels], [("Village", "settlement")])

    def test_missing_translation_is_not_replaced_with_an_internal_id(self):
        self.assertEqual(make_labels([location("Unknown")], [], {}), [])

    def test_original_pointer_can_supply_a_missing_named_place(self):
        pointer = {**location("PointerPlace", -70, 80), "PointerType": 5, "SaveFlag": "Location_PointerPlace"}
        labels = make_labels([pointer], [], {"PointerPlace": "Fixture plateau"})
        self.assertEqual((labels[0]["name"], labels[0]["x"], labels[0]["z"]), ("Fixture plateau", -70, 80))

    def test_invalid_positions_and_visibility_are_rejected(self):
        for x, z, level in [(math.nan, 0, 4), (0, math.inf, 4), (6001, 0, 4), (0, -5001, 4), (0, 0, 3)]:
            with self.subTest(x=x, z=z, level=level), self.assertRaises(ValueError):
                make_labels([location("Place", x, z, level)], [], {"Place": "Fixture Place"})

    def test_unprintable_names_and_malformed_entries_are_rejected(self):
        with self.assertRaises(ValueError):
            make_labels([location("Place")], [], {"Place": "Bad\x01name"})
        with self.assertRaises(ValueError):
            make_labels([None], [], {})


if __name__ == "__main__":
    unittest.main(verbosity=2)
