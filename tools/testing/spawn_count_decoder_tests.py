"""Focused synthetic tests for native species-count decoding and guard policy."""
from __future__ import annotations

import hashlib
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/coo"))
import generate_open_world_profiles as gen
import spawn_count_policy as counts


def _align(value: int, alignment: int = 16) -> int:
    return (value + alignment - 1) & -alignment


class BlobBuilder:
    def __init__(self, size: int = 0x10000, cursor: int = 0x300):
        self.data = bytearray(size)
        self.cursor = cursor

    def reserve(self, size: int) -> int:
        result = _align(self.cursor)
        self.cursor = result + size
        return result

    def array(self, descriptor: int, element_class: int, stride: int, count: int) -> list[int]:
        if not count:
            struct.pack_into("<Qq", self.data, descriptor, 0, 0)
            return []
        header = self.reserve(20 + stride * count) + 4
        struct.pack_into("<I", self.data, header - 4, 0x80809FBD)
        struct.pack_into("<QI", self.data, header, count, element_class)
        struct.pack_into("<Qq", self.data, descriptor, count, header - (descriptor + 8))
        first = header + 16
        return [first + index * stride for index in range(count)]


def template_blob(resources: list[int]) -> bytes:
    build = BlobBuilder(size=0x1000, cursor=0x80)
    for row, resource in zip(build.array(16, 0x80809C04, 12, len(resources)), resources):
        struct.pack_into("<I", build.data, row, resource)
    return bytes(build.data)


def resource_blob(types: list[int]) -> bytes:
    build = BlobBuilder(size=0x1000, cursor=0x180)
    body = 0x40
    struct.pack_into("<q", build.data, 24, body - 24)
    struct.pack_into("<I", build.data, body - 4, 0x808038A1)
    constraints = build.array(body + 0xA8, 0x808038AA, 24, 1)
    struct.pack_into("<I", build.data, constraints[0] + 4, 0x26170C92)
    values = build.array(constraints[0] + 8, 0x80800070, 4, len(types))
    for row, value in zip(values, types):
        struct.pack_into("<I", build.data, row, value)
    return bytes(build.data)


def source_blob(key: int, categories: list[list[list[dict]]]) -> bytes:
    """Build categories[category][variant][choice] for the exact native layouts."""
    build = BlobBuilder()
    body = 0x40
    struct.pack_into("<q", build.data, 24, body - 24)
    struct.pack_into("<I", build.data, body - 4, 0x8080948F)
    struct.pack_into("<IHH", build.data, body + 48, key, 1, 0)
    category_rows = build.array(body + 0xA8, 0x80808356, 104, len(categories))
    pending = []
    for category_row, variants in zip(category_rows, categories):
        if len(variants) != 6:
            raise ValueError("fixture must contain all six native variants")
        for variant_index, choices in enumerate(variants):
            rows = build.array(category_row + 8 + variant_index * 16, 0x80808358, 24, len(choices))
            pending.extend(zip(rows, choices))
    for row, choice in pending:
        # Leave room beyond the 8-byte relative pointer at +0x78 so the
        # following selector's class word cannot overlap that pointer.
        entity_body = build.reserve(0x90)
        struct.pack_into("<I", build.data, entity_body - 4, 0x808099D8)
        struct.pack_into("<I", build.data, entity_body, choice["entity"])
        struct.pack_into("<I", build.data, row + 12, choice.get("weight", 1))
        struct.pack_into("<q", build.data, row, entity_body - row)
        wrapper_class = choice.get("wrapper")
        if wrapper_class is None:
            selector = build.reserve(0x20)
            struct.pack_into("<q", build.data, entity_body + 0x78, selector - (entity_body + 0x78))
        else:
            wrapper = build.reserve(8)
            selector = build.reserve(0x20)
            struct.pack_into("<I", build.data, wrapper - 4, wrapper_class)
            struct.pack_into("<q", build.data, wrapper, selector - wrapper)
            struct.pack_into("<q", build.data, entity_body + 0x78, wrapper - (entity_body + 0x78))
        struct.pack_into("<I", build.data, selector - 4, 0x808038A0)
        attributes = choice.get("attributes", [])
        attribute_rows = build.array(selector + 16, 0x8080389F, 8, len(attributes))
        for attribute_row, (attribute_key, value) in zip(attribute_rows, attributes):
            struct.pack_into("<II", build.data, attribute_row, attribute_key, value)
    return bytes(build.data)


def six_variants(*choices: dict) -> list[list[dict]]:
    return [[dict(choice) for choice in choices] for _ in range(6)]


class NativeDecoderChecks(unittest.TestCase):
    key = 0x12345678
    source_tag = 101
    entity = 201
    goblin = counts.fnv1("goblin")
    hobgoblin = counts.fnv1("hobgoblin")
    minor = counts.fnv1("minor")

    def setUp(self):
        gen.template_type_evidence.cache_clear()
        self.group = {"key": self.key, "slots": [
            {"index": 0, "type": 1, "descriptor": self.source_tag},
        ]}
        self.blobs = {}

    def read(self, tag: int):
        return self.blobs[tag]

    def decode(self, categories: list[list[list[dict]]]):
        self.blobs[self.source_tag] = (0x80809C36, source_blob(self.key, categories))
        with patch.object(gen.package_read, "read", side_effect=self.read):
            return gen.source_count_choices(self.group, 0)

    def set_template(self, entity: int, resources: list[tuple[int, list[int]]]):
        self.blobs[entity] = (0x80809C0F, template_blob([tag for tag, _ in resources]))
        for tag, types in resources:
            self.blobs[tag] = (0x80809C36, resource_blob(types))

    def test_concrete_selector_narrows_polymorphic_template(self):
        self.set_template(self.entity, [(301, [self.goblin, self.hobgoblin])])
        choice = {"entity": self.entity, "attributes": [
            (0x26170C92, self.hobgoblin), (0xB10F785D, self.minor),
        ]}
        decoded = self.decode([six_variants(choice)])
        self.assertEqual({row["species"] for row in decoded[0]}, {"hobgoblin"})
        self.assertEqual({row["rank"] for row in decoded[0]}, {"minor"})
        self.assertTrue(all(row["template_evidence_sha256"] for row in decoded[0]))

    def test_selector_outside_template_options_becomes_unknown(self):
        self.set_template(self.entity, [(301, [self.goblin])])
        choice = {"entity": self.entity, "attributes": [
            (0x26170C92, self.hobgoblin), (0xB10F785D, self.minor),
        ]}
        self.assertEqual({row["species"] for row in self.decode([six_variants(choice)])[0]}, {None})

    def test_both_supported_selector_wrappers_decode(self):
        self.set_template(self.entity, [(301, [self.goblin])])
        for wrapper in (0x80807EB6, 0x80804B8B):
            with self.subTest(wrapper=f"{wrapper:08X}"):
                gen.template_type_evidence.cache_clear()
                choice = {"entity": self.entity, "wrapper": wrapper, "attributes": [
                    (0x26170C92, self.goblin), (0xB10F785D, self.minor),
                ]}
                decoded = self.decode([six_variants(choice)])
                self.assertEqual({row["species"] for row in decoded[0]}, {"goblin"})

    def test_conflicting_duplicate_selector_attributes_rejected(self):
        self.set_template(self.entity, [(301, [self.goblin, self.hobgoblin])])
        choice = {"entity": self.entity, "attributes": [
            (0x26170C92, self.goblin), (0x26170C92, self.hobgoblin),
        ]}
        with self.assertRaisesRegex(ValueError, "conflicting native choice selector attributes"):
            self.decode([six_variants(choice)])

    def test_zero_weight_excluded_and_all_six_variants_retained(self):
        self.set_template(self.entity, [(301, [self.goblin])])
        second_entity = 202
        self.set_template(second_entity, [(302, [self.hobgoblin])])
        zero = {"entity": self.entity, "weight": 0, "attributes": [(0xB10F785D, self.minor)]}
        positive = {"entity": second_entity, "weight": 1, "attributes": [(0xB10F785D, self.minor)]}
        decoded = self.decode([six_variants(zero, positive)])
        self.assertEqual(len(decoded[0]), 6)
        self.assertEqual({row["entity"] for row in decoded[0]}, {f"{second_entity:08X}"})
        self.assertEqual({row["species"] for row in decoded[0]}, {"hobgoblin"})

    def test_multiple_resource_type_constraints_form_conservative_union(self):
        self.set_template(self.entity, [(301, [self.goblin]), (302, [self.hobgoblin])])
        choice = {"entity": self.entity, "attributes": [(0xB10F785D, self.minor)]}
        decoded = self.decode([six_variants(choice)])
        self.assertEqual({row["species"] for row in decoded[0]}, {None})
        with patch.object(gen.package_read, "read", side_effect=self.read):
            types, digest = gen.template_type_evidence(self.entity)
        self.assertEqual(types, frozenset((self.goblin, self.hobgoblin)))
        self.assertRegex(digest, r"^[0-9a-f]{64}$")


class CrossCategoryGuardChecks(unittest.TestCase):
    def setUp(self):
        self.target = gen.Target("test", "Test", "test", 1, 0, ((0, 1),), (), ())
        self.group = {"key": 0x12345678}

    def resolve(self, choices: list[list[dict]], digest: str | None = None):
        pin = {"count_choices_sha256": digest or gen.count_choices_digest(choices)}
        with patch.object(gen, "source_count_choices", return_value=choices):
            return gen.species_request_overrides(self.target, self.group, 0, 2, pin)

    def test_major_cap_suppresses_second_major_category(self):
        choices = [
            [{"entity": "1", "template_evidence_sha256": "a", "species": "knight", "rank": "major"}],
            [{"entity": "2", "template_evidence_sha256": "b", "species": "goblin", "rank": "major"}],
        ]
        self.assertEqual(self.resolve(choices), (1, 0))

    def test_same_fixed_species_in_both_categories_suppresses_second(self):
        choices = [
            [{"entity": "1", "template_evidence_sha256": "a", "species": "minotaur", "rank": "minor"}],
            [{"entity": "2", "template_evidence_sha256": "b", "species": "minotaur", "rank": "minor"}],
        ]
        self.assertEqual(self.resolve(choices), (1, 0))

    def test_count_choice_digest_pin_detects_decoder_change(self):
        choices = [
            [{"entity": "1", "template_evidence_sha256": "a", "species": "goblin", "rank": "minor"}],
            [{"entity": "2", "template_evidence_sha256": "b", "species": "harpy", "rank": "minor"}],
        ]
        with self.assertRaisesRegex(ValueError, "templates or selectors changed"):
            self.resolve(choices, hashlib.sha256(b"stale").hexdigest())


if __name__ == "__main__":
    unittest.main()
