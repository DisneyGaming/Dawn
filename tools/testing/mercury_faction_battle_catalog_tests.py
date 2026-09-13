"""Focused installed-package checks for Mercury patrol/source evidence.

Run directly with the repository read-only installed-package adapter. These
checks never attach to a process, publish a source, or write package bytes.
"""
import struct
import sys
import unittest
import json
from pathlib import Path

from mercury_faction_battle_catalog import CATALOG_KEYS, CHEST_SOURCES, catalog, source_choices, tactical_rows
from mercury_faction_battle_presentation import incident_presentation, presentation_variant
from mercury_faction_battle_ambient_relation import extract as ambient_comparison, installed_components

from pkg import Reader


class MercuryCatalogChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.reader = Reader()

    def read(self, tag, expected):
        data, cls = self.reader.read_tag(tag)
        self.assertEqual(cls, expected)
        return data

    def test_catalog_scope_and_shared_family_identity(self):
        data = self.read(0x80F55240, 0x80809440)
        result = catalog(data)
        self.assertEqual(len(result), 8)
        self.assertEqual(tuple(tuple(int(c['registry'], 16) for c in row['candidates'])
                               for row in result), CATALOG_KEYS)
        self.assertEqual(result[0]['label'], 'Mercury Destination Lighthouse FULL Public Events')
        self.assertEqual(result[1]['label'], 'Mercury Destination Lighthouse SKIRMISH Silent Events')
        self.assertEqual(result[1]['group_hash'], result[2]['group_hash'])
        self.assertEqual(result[4]['group_hash'], result[5]['group_hash'])
        self.assertEqual(result[0]['candidates'][0]['rally_registry'], '85C38F77')
        self.assertTrue(all(c['bubble_hash'] == 'A83A9175' for row in result for c in row['candidates']))

    def test_catalog_scalars_are_retained_without_timer_units(self):
        result = catalog(self.read(0x80F55240, 0x80809440))
        self.assertEqual([r['scalars_24_28_2c_30'] for r in result], [
            [22, 0, 1, 1], [14, 0, 2, 1], [14, 0, 2, 1], [11, 2, 4, 1],
            [11, 0, 6, 1], [11, 0, 6, 1], [10, 2, 3, 1], [11, 2, 5, 1]])
        self.assertNotIn('seconds', str(result))

    def test_chest_all_native_variants_and_weighted_choices(self):
        expected = {
            0: [[('80C0D09F', 1, '7E35F434', '9A3D2CCB')]],
            2: [[('80C0D038', 7, '7E35F434', 'D09AD130'), ('80C0D038', 1, '7E35F434', '4A3554B4')]],
            12: [[('80C0FA98', 1, '72300E38', '4A3554B4'), ('80C19B1F', 1, '72300E38', '4A3554B4'),
                  ('80C1ACAF', 1, '72300E38', '4A3554B4')],
                 [('80C1A52D', 1, '72300E38', 'D09AD130'), ('80C1A8E4', 1, '72300E38', 'D09AD130')]],
        }
        for _, _, sources, _ in CHEST_SOURCES:
            for slot, descriptor in sources:
                with self.subTest(descriptor=f'{descriptor:08X}'):
                    redirect = self.read(descriptor, 0x80809B14)
                    resource = struct.unpack_from('<I', redirect, 12)[0]
                    source = source_choices(self.read(resource, 0x80809C36))
                    self.assertIsNone(source['requested_actors'])
                    self.assertIsNone(source['host_variant_selection'])
                    for variant in range(6):
                        actual = []
                        for category in source['categories']:
                            self.assertTrue(category['all_six_choices_identical'])
                            choices = []
                            for choice in category['variants'][variant]:
                                attrs = {a['key']: a['value'] for a in choice['selector_attributes']}
                                choices.append((choice['entity'], choice['weight'], attrs['6EECD523'], attrs['B10F785D']))
                            actual.append(choices)
                        self.assertEqual(actual, expected[slot])

    def test_tactical_rows_retain_scope_without_source_assignment(self):
        for _, key, _, descriptor in CHEST_SOURCES:
            resource = struct.unpack_from('<I', self.read(descriptor, 0x80809B14), 12)[0]
            result = tactical_rows(self.read(resource, 0x80809C36))
            self.assertIsNone(result['source_to_row_assignment'])
            self.assertEqual([r['hash_index'] for r in result['rows']], [2, 3, 5, 6, 7, 8])
            self.assertEqual([r['tasks'] for r in result['rows']], [
                [{'registry': f'{key:08X}', 'type': 45, 'slot': slot}] for slot in [31, 32, 33, 34, 36, 37]])

    def test_corrupt_catalog_header_and_relative_pointer_rejected(self):
        original = self.read(0x80F55240, 0x80809440)
        for offset, value in ((16, -64), (16, len(original)), (8, 300001), (24, 0)):
            data = bytearray(original)
            struct.pack_into('<q', data, offset, value)
            with self.subTest(offset=offset, value=value), self.assertRaises(ValueError):
                catalog(data)

    def test_unrelated_typed_resources_cannot_become_sources(self):
        data = self.read(0x80F5BF61, 0x80809C36)  # authored timer resource
        with self.assertRaises(ValueError):
            source_choices(data)

    def test_selector_class_corruption_rejected(self):
        data = bytearray(self.read(0x80F5BF39, 0x80809C36))
        struct.pack_into('<I', data, 0x97C, 0x808099D8)
        with self.assertRaises(ValueError):
            source_choices(data)

    def test_announcement_is_reached_through_incident_presentation_list(self):
        for tag in (0x80B9E5BF, 0x81327CD4):
            result = incident_presentation(self.read(tag, 0x80807C9B), 0xA2DD920A)
            self.assertEqual(result['incident_row'], 4852)
            self.assertEqual(result['type'], 0)
            self.assertEqual(result['presentation_list_index'], 157)
            self.assertEqual(result['presentation_hashes'], ['A2DD920A'])
            self.assertEqual(result['payload_class'], '80808751')
            self.assertEqual(result['payload_first_eight_bytes'], '0000000000000000')

    def test_announcement_native_recipient_and_channel_fields(self):
        result = presentation_variant(self.read(0x80B9E5E3, 0x80806485), 0xA2DD920A)
        self.assertEqual(result['presentation_row'], 287)
        self.assertFalse(result['native_delivery_and_receipt_qualified'])
        self.assertEqual(len(result['variants']), 1)
        row = result['variants'][0]
        self.assertEqual(row['localized_hash'], 'A2DD920A')
        self.assertEqual(row['localized_container'], '80B9E37E')
        self.assertEqual(row['recipient_predicate'], '1124697D')
        self.assertEqual(row['channel'], '811C9DC5')
        self.assertEqual(row['argument_resource'], 'FFFFFFFF')
        self.assertEqual(row['excluded_recipient_predicates'], [])
        self.assertEqual((row['local_player_filter'], row['native_text_mode_58'], row['proximity_radius']), (0, 0, 0))
        self.assertEqual(row['proximity_subject'], '811C9DC5')
        self.assertEqual(row['enablement_hash'], '811C9DC5')

    def test_invalid_presentation_list_is_rejected(self):
        original = self.read(0x80B9E5BF, 0x80807C9B)
        for index in (-1, 3193):
            data = bytearray(original)
            struct.pack_into('<h', data, 194208 + 16, index)
            with self.subTest(index=index), self.assertRaises(ValueError):
                incident_presentation(data, 0xA2DD920A)

    def test_unqualified_recipient_exclusion_cannot_be_silently_ignored(self):
        data = bytearray(self.read(0x80B9E5E3, 0x80806485))
        struct.pack_into('<Q', data, 168912 + 0x48, 1)
        with self.assertRaises(ValueError):
            presentation_variant(data, 0xA2DD920A)

    def test_all_ordinary_and_hotspot_variants_are_comparisons_not_waves(self):
        inventory = installed_components(self.reader)
        result = ambient_comparison(self.reader, inventory)
        self.assertEqual(len(result['sources']), 72)
        self.assertEqual(len({s['registry'] for s in result['sources']}), 37)
        self.assertEqual(len({s['registry'] for s in result['sources'] if s['hotspot_name']}), 15)
        self.assertTrue(result['all_source_variants_identical'])
        self.assertIsNone(result['faction_battle_source_order'])
        self.assertIsNone(result['hotspot_activation_condition'])
        self.assertTrue(all(p['authored_selection_relationship'] is None for p in result['name_comparisons']))
        tower = next(s for s in result['sources'] if s['resource'] == '80F5B6C8')
        for variant in tower['categories'][0]['variants']:
            self.assertEqual(len(variant), 1)
            self.assertEqual(variant[0]['selector_wrapper'], '80804B8B')
            self.assertEqual(variant[0]['entity'], '80C0FA98')
            attrs = {a['key']: a['value'] for a in variant[0]['selector_attributes']}
            self.assertEqual((attrs['6EECD523'], attrs['B10F785D']), ('72300E38', '4A3554B4'))

    def test_requested_density_fits_receipt_and_observer_capacity(self):
        inventory = installed_components(self.reader)
        result = ambient_comparison(self.reader, inventory)
        selected = {
            '74337EDD', 'EB1E8934', 'B3CBA385', '2571C34D', '9D083869', '9692BB5E',
            '1ED6087A', 'FB7F2889', '1780D86F', '90EFDE28', 'CF2196EA', '8C756CC3',
            'BBF1BA51', '9B219BF3', 'CCF03E8D', '0EFE61CB',
        }
        # Count every authored choice in every category as if it materialized;
        # this is stricter than weighted selection and avoids inferring retail
        # spawn behavior from the package layout.
        def ceiling(source):
            return max(sum(len(category['variants'][variant]) for category in source['categories'])
                       for variant in range(6))
        self.assertEqual(max(ceiling(source) for source in result['sources']), 3)
        self.assertLessEqual(max(ceiling(source) for source in result['sources']
                                 if source['registry'] in selected), 3)
        self.assertLessEqual((4 + 5) * 3, 64)       # one large war source generation
        self.assertLessEqual((8 * 3 + 8 * 4) * 3, 256)  # simultaneous initial admissions
        self.assertLessEqual((8 * 3 + 8 * 4) * 3 * 3, 1024)  # birth/death/retirement burst

    def test_adjacent_retreat_announcement_identity_without_completion_claim(self):
        result = incident_presentation(self.read(0x80B9E5BF, 0x80807C9B), 0x8753E5BA)
        self.assertEqual(result['incident_row'], 3987)
        self.assertEqual(result['presentation_list_index'], 158)
        self.assertEqual(result['presentation_hashes'], ['8753E5BA'])
        row = presentation_variant(self.read(0x80B9E5E3, 0x80806485), 0x8753E5BA)
        self.assertEqual(row['presentation_row'], 296)
        self.assertEqual(row['variants'][0]['localized_container'], '80B9E37E')


if __name__ == '__main__':
    unittest.main()
