"""Verify the two installed strike catalogs' Grandmaster enemy substitutions.

Reads exact source descriptors with the existing package reader. It does not
modify packages and deliberately does not assign Champion subtypes: the native
entity resources in this build expose no recovered display-name/property proof.
"""
from pathlib import Path
import re
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/testing'))
from pkg import Reader
from mercury_faction_battle_catalog import source_choices

# strike, registry, source, descriptor, category, standard entity, GM entity
ROWS = (
('bond',0xC95ECB1A,16,0x80F54368,0x19C57E66,0x80F58985,0x8157862E),
('bond',0xC95ECB1A,26,0x80F54386,0x19C57E65,0x80F587BD,0x81578798),
('bond',0xC95ECB1A,28,0x80F5438C,0x19C57E65,0x80F587BD,0x81578798),
('bond',0xC95ECB1A,42,0x80F543B6,0x19C57E65,0x80F587BD,0x81578798),
('bond',0xC95ECB1A,46,0x80F543C2,0x19C57E65,0x80F58985,0x8157862E),
('bond',0xC95ECB1A,48,0x80F543C8,0x19C57E65,0x80F587BD,0x81578798),
('bond',0xC95ECB1A,49,0x80F543CB,0x19C57E65,0x80F587BD,0x81578798),
('bond',0xC95ECB1A,50,0x80F543CE,0x19C57E65,0x80F587BD,0x81578798),
('bond',0xC95ECB1A,52,0x80F543D4,0x19C57E65,0x80F58985,0x8157862E),
('bond',0xC95ECB1A,57,0x80F543E3,0x19C57E65,0x80F58985,0x8157862E),
('bond',0xC95ECB1A,58,0x80F543E6,0x19C57E65,0x80F58985,0x8157862E),
('bond',0xC95ECB1A,59,0x80F543E9,0x19C57E65,0x80F58985,0x8157862E),
('bond',0xC95ECB1A,60,0x80F543EC,0x19C57E65,0x80F58985,0x8157862E),
('bond',0xC95ECB1A,61,0x80F543EF,0x19C57E65,0x80F587BD,0x81578798),
('bond',0x2CB86C0F,39,0x80F547AC,0x19C57E66,0x80F587BD,0x81578798),
('bond',0x2CB86C0F,41,0x80F547B2,0x19C57E66,0x80F58985,0x8157862E),
('bond',0x2CB86C0F,44,0x80F547BB,0x19C57E66,0x80F58985,0x8157862E),
('bond',0x2CB86C0F,46,0x80F547C1,0x19C57E65,0x80F58985,0x8157862E),
('bond',0x2CB86C0F,50,0x80F547CD,0x19C57E65,0x80F587BD,0x81578798),
('bond',0x2CB86C0F,51,0x80F547D0,0x19C57E65,0x80F587BD,0x81578798),
('bond',0x2CB86C0F,54,0x80F547D9,0x19C57E65,0x80F587BD,0x81578798),
('bond',0x2CB86C0F,56,0x80F547DF,0x19C57E66,0x80F58985,0x8157862E),
('bond',0x2CB86C0F,57,0x80F547E2,0x19C57E66,0x80F58985,0x8157862E),
('pact',0xA5F083B5,4,0x80F54B7C,0x6FC008B5,0x80C1ACAF,0x8161FED1),
('pact',0xA5F083B5,19,0x80F54BA9,0xDFD659AC,0x80C1A8E4,0x8161FED1),
('pact',0xA5F083B5,38,0x80F54BE2,0xA21827B6,0x80C0D298,0x8157862E),
('pact',0xA5F083B5,42,0x80F54BEE,0x7817E226,0x80C1A8E4,0x8161FED1),
('pact',0xA5F083B5,44,0x80F54BF4,0x6116E417,0x80C0D09F,0x81578798),
('pact',0xA5F083B5,56,0x80F54C18,0x7817E226,0x80C1A8E4,0x8161FED1),
('pact',0xA5F083B5,57,0x80F54C1B,0xA21827B6,0x80C0D298,0x8157862E),
('pact',0xA5F083B5,65,0x80F54C33,0xA21827B6,0x80C0D298,0x8157862E),
('pact',0xA5F083B5,73,0x80F54C4B,0x6116E417,0x80C0D09F,0x81578798),
('pact',0xA5F083B5,74,0x80F54C4E,0x6116E417,0x80C0D09F,0x81578798),
('pact',0xA5F083B5,80,0x80F54C60,0x6116E417,0x80C0D09F,0x81578798),
('pact',0xA5F083B5,81,0x80F54C63,0x6116E417,0x80C0D09F,0x81578798),
('pact',0x588E5FB9,2,0x80F54EA8,0xD3D162D4,0x80C19B1F,0x8161FED1),
('pact',0x588E5FB9,2,0x80F54EA8,0x0EA2CE68,0x80C19B1F,0x8161FED1),
('pact',0x588E5FB9,20,0x80F54EDE,0xB4009D19,0x80C0D0BF,0x81578798),
('pact',0x588E5FB9,25,0x80F54EED,0xBF95E58C,0x80C0FA98,0x8161FED1),
('pact',0x588E5FB9,27,0x80F54EF3,0xBF95E58C,0x80C0FA98,0x8161FED1),
('pact',0xCC7A090D,14,0x80F5513F,0xBF95E58C,0x80C0FA98,0x8161FED1),
)

class NightfallEnemyVariants(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.reader = Reader()

    def test_exact_installed_variant_zero_and_five(self):
        for strike, registry, source, descriptor, category, standard, gm in ROWS:
            data, source_class = self.reader.read_tag(descriptor)
            self.assertEqual(source_class, 0x80809C36)
            categories = {int(row['key'],16): row for row in source_choices(data)['categories']}
            self.assertIn(category, categories)
            variants = categories[category]['variants']
            self.assertEqual([int(x['entity'],16) for x in variants[0]], [standard])
            self.assertEqual([int(x['entity'],16) for x in variants[5]], [gm])
            self.assertNotEqual(variants[0], variants[5], (strike, registry, source))
            self.assertEqual(self.reader.read_tag(gm)[1], 0x80809C0F)

    def test_catalog_tables_equal_package_evidence(self):
        pattern = re.compile(r'\{0x([0-9A-F]{8})U,(\d+),0x([0-9A-F]{8})U,0x([0-9A-F]{8})U,0x([0-9A-F]{8})U\}')
        for strike, path in (
            ('pact', ROOT/'Dawn/src/state/activity/strike_pact/catalog_all.h'),
            ('bond', ROOT/'Dawn/src/state/activity/strike_bond/catalog.h')):
            text = path.read_text().split('kGrandmasterEnemySubstitutions{{',1)[1].split('}};',1)[0]
            actual = {tuple(int(x,16) if i != 1 else int(x) for i,x in enumerate(match))
                      for match in pattern.findall(text)}
            expected = {(registry,source,category,standard,gm)
                        for name,registry,source,_descriptor,category,standard,gm in ROWS if name==strike}
            self.assertEqual(actual, expected)

    def test_every_allowlisted_source_has_a_nonempty_variant_five_for_every_category(self):
        seen = set()
        for strike, registry, source, descriptor, *_ in ROWS:
            if (strike, registry, source) in seen:
                continue
            seen.add((strike, registry, source))
            data, source_class = self.reader.read_tag(descriptor)
            self.assertEqual(source_class, 0x80809C36)
            categories = source_choices(data)['categories']
            self.assertTrue(categories)
            self.assertTrue(all(category['variants'][5] for category in categories),
                            (strike, registry, source, descriptor))

    def test_source_counts_keep_mixed_category_identity_visible(self):
        pact = {(registry,source) for name,registry,source,*_ in ROWS if name=='pact'}
        bond = {(registry,source) for name,registry,source,*_ in ROWS if name=='bond'}
        self.assertEqual((len(pact),len(bond)), (17,23))
        self.assertEqual(sum(1 for row in ROWS if row[0]=='pact' and row[1:3]==(0x588E5FB9,2)),2)

if __name__ == '__main__': unittest.main()
