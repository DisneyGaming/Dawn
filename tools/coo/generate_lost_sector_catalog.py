"""Generate the first package-pinned Lost Sector capability catalog.

Mercury and Mars use exact registry/source/fallback-rule descriptors and native
point closures. Stage order and initial tactical objectives are an explicitly
documented host reconstruction from package role labels plus period guides; it
is not represented as a recovered retail script.
"""
from __future__ import annotations

import hashlib
import json
import sys
from pathlib import Path
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/coo"))

import generate_open_world_profiles as gen
import spawn_count_policy as counts
import squad_cohort_policy as cohorts


QUOTAS_PATH = ROOT / "tools/coo/lost_sector_encounter_quotas.json"
FOOTAGE_REVIEW_PATH = ROOT / "tools/coo/lost_sector_footage_review.json"
QUOTA_TARGET_MINIMUM = 0
QUOTA_TARGET_MAXIMUM = 63


# Reviewed package point-list/container dependencies. The generator replays the
# complete selected source-rule GUID closure and rejects any byte/owner drift.
POINT_DEPENDENCIES = {
    0xA27443E8: (["80BF01E9", "80F32F95"], ["80F32F96"]),
    0x2E3D2EB4: (["80BF1321", "80F33EF0"], ["80F33EF1"]),
    0x98B63236: (["80BF6FDF", "80F33EF0"], ["80F33EF1"]),
    0x5BA616DA: (["80BF1563", "80F3400A"], ["80F3400B"]),
    0x6717656F: (["80B43307", "80C07736"], ["80EE0662"]),
    0xD7BD9746: (["80B43307", "80C03BEB"], ["80EE0662"]),
    0xBB69D2E9: (["80C08DCC", "80C309ED"], ["80EE10CD"]),
    0x3F8AF55C: (["80C09223", "80C31198", "80EE12FD"], ["80EE133B", "80EE133D"]),
    0x2F8DB58A: (["80C0A4EC", "80C4C979"], ["80EE183D"]),
    0x719085A0: (["80C08522", "80C4C979"], ["80EE183D"]),
    0x100F6578: (["80C31947", "80F6849F"], ["80F684D9"]),
    0xE8346A52: (["80C0A935", "80F6849F"], ["80F684D9"]),
    0x265E16C7: (["80BD352B"], []), 0xDB5D8740: (["80BD7117", "80ED5B87"], ["81544CBD"]),
    0x319CB306: (["80BD3680"], []), 0xA4DFFD0C: (["80BD36DB", "80ED5D5E"], ["80ED5B5C"]),
    0xE2E83849: (["80BD36AD"], []), 0x33C30847: (["80BD8A35", "80C37000"], ["80ED5519"]),
    0x87678E81: (["80BD6B9F"], []), 0xCEA81A19: (["80BD6B39"], []),
    0xDF80EDAD: (["80BD6B22"], []),
    0x3A80D8D4: (["80F4E134", "80F5E6AE"], ["80F4FF2E"]),
    0xD893B268: (["80F4E134", "80F5028B", "80F5E667"], ["80F4FF2E", "80F5028C"]),
    0x7BFEF9E6: (["80BFDD7B", "80EA8D93"], ["80EA8C46", "80EA8C4B", "80EA8D94", "80EA9154", "80EA9176", "80EA922B", "80EA9846", "80F6C458", "80F6C531"]),
    0x8DA9816E: (["80EA8D93", "80F73A17"], ["80EA8D94"]),
    0x16833C32: (["80F73E4A"], []), 0x4F05045F: (["80F73EAF"], []),
    0x7E21B2C3: (["80F6CAFC", "80F73E67"], ["80F6CAFD"]),
    0x51AA084F: (["80FD5C5F"], []), 0xB8D9E2F8: (["80FD5C96"], []),
    0xCED21737: (["80FD5CCE"], []),
    0xAD5D1D0F: (["80FBED24", "80FD8539"], ["80FBED25"]),
    0xD8755C9C: (["80FBED24", "80FD8583"], ["80FBED25"]),
    0xED59DF6F: (["80FD9604"], []), 0x9A24C39A: (["80FD9123"], []),
    0x5B603F1E: (["80FDC2F8"], []), 0x76A432A7: (["80FDC34A"], []),
    0xBD63AFEC: (["80FDC3AB"], []),
    0xB846777D: (["80F24DDE"], []),
    0x74E16154: (["80F251B5"], []),
    0xD7FF797A: (["80F2549D"], []),
}

OMITTED_SOURCES = {
    (0x69A1B17C, 8): "unsupported_native_three_category_helper",
    **{(0xB846777D, source): "non_lost_sector_or_ascendant_overlay"
       for source in (0, 1, 2, 3, 4, 5, 45, 48, 49)},
    **{(0x74E16154, source): "ascendant_overlay"
       for source in (19, 23, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34,
                      35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48)},
    **{(0xD7FF797A, source): "ascendant_overlay"
       for source in (24, 28)},
}


SECTORS = (
    {
        "namespace": "mercury", "activity": "mercury_freeroam", "scenario": 0x80F4696A,
        "name": "Pariah's Refuge", "bubble": 16,
        "groups": ((0x3A80D8D4, 0x80F5E6F5), (0xD893B268, 0x80F5E6A5)),
        "stages": (
            ((0x3A80D8D4, (1, 2, 3, 4)),),
            ((0x3A80D8D4, (5, 6, 7, 8)),),
            ((0x3A80D8D4, (9, 10, 11)),),
            ((0xD893B268, (4, 5)),),
            ((0xD893B268, (6, 7, 8, 9)),),
            ((0xD893B268, (10, 11)),),
            ((0xD893B268, (12, 13)),),
            ((0xD893B268, (2,)),),
        ),
        "boss": (0xD893B268, 2),
    },
    {
        "namespace": "mars", "activity": "polaris_freeroam", "scenario": 0x80F6AB20,
        "name": "Core Terminus", "bubble": 2,
        "groups": ((0x8DA9816E, 0x80F73A65), (0x7BFEF9E6, 0x80F73AD0)),
        "stages": (
            ((0x8DA9816E, tuple(range(13))),),
            ((0x7BFEF9E6, (2,)),),
            ((0x7BFEF9E6, (3, 4, 5, 6)),),
            ((0x7BFEF9E6, (7, 8)),),
            ((0x7BFEF9E6, (9, 10, 11, 12)),),
            ((0x7BFEF9E6, (13, 14, 15, 16, 17, 18, 0)),),
        ),
        "boss": (0x7BFEF9E6, 0),
    },
    {
        "namespace": "mars", "activity": "polaris_freeroam", "scenario": 0x80F6AB20,
        "name": "Ma'adim Subterrane", "bubble": 6,
        "groups": ((0x16833C32, 0x80F73E64), (0x4F05045F, 0x80F73ED8), (0x7E21B2C3, 0x80F73EAC)),
        "stages": (
            ((0x16833C32, (1, 2, 3, 4)),),
            ((0x4F05045F, (1, 2, 3, 4, 5, 6, 8)),),
            ((0x7E21B2C3, (2,)),),
            ((0x7E21B2C3, (5, 6, 7, 8, 9, 10)),),
            ((0x7E21B2C3, (11, 12, 3)),),
        ),
        "boss": (0x7E21B2C3, 3),
    },
    {
        "namespace": "titan", "activity": "fleet_freeroam", "scenario": 0x80B3E142,
        "name": "Methane Flush", "bubble": 3,
        "groups": ((0xA27443E8, 0x80BF014B),),
        "stages": (
            ((0xA27443E8, (5, *range(6, 19))),), ((0xA27443E8, tuple(range(19, 27))),),
            ((0xA27443E8, tuple(range(27, 32))),), ((0xA27443E8, (32, 33, 34, 2, 3)),),
        ), "boss": (0xA27443E8, 3),
    },
    {
        "namespace": "titan", "activity": "fleet_freeroam", "scenario": 0x80B3E142,
        "name": "DS Quarters-2", "bubble": 8,
        "groups": ((0x98B63236, 0x80BF6158), (0x2E3D2EB4, 0x80BF60E6)),
        "stages": (
            ((0x98B63236, tuple(range(13))),), ((0x2E3D2EB4, (2, 3)),),
            ((0x2E3D2EB4, (4, 5, 6, 7)),), ((0x2E3D2EB4, (8, 9)),),
            ((0x2E3D2EB4, (10, 11, 12, 13)),),
            ((0x2E3D2EB4, (14, 15, 16, 17, 18, 19, 0)),),
        ), "boss": (0x2E3D2EB4, 0),
    },
    {
        "namespace": "titan", "activity": "fleet_freeroam", "scenario": 0x80B3E142,
        "name": "Cargo Bay 3", "bubble": 9,
        "groups": ((0x5BA616DA, 0x80BF66AC),),
        "stages": (
            ((0x5BA616DA, (19, *range(3, 15))),),
            ((0x5BA616DA, (*range(15, 19), 20, 2, 21)),),
        ), "boss": (0x5BA616DA, 21),
    },
    {
        "namespace": "io", "activity": "eden_freeroam", "scenario": 0x80B56B1B,
        "name": "Sanctum of Bones", "bubble": 5,
        "groups": ((0x265E16C7, 0x80BD760C), (0xDB5D8740, 0x80BD76CA)),
        "stages": (
            ((0x265E16C7, tuple(range(1, 8))),), ((0xDB5D8740, (4, *range(5, 15))),),
            ((0xDB5D8740, (15, 16, 17)),), ((0xDB5D8740, (18, 19, 20, 2)),),
        ), "boss": (0xDB5D8740, 2),
    },
    {
        "namespace": "io", "activity": "eden_freeroam", "scenario": 0x80B56B1B,
        "name": "Aphix Conduit", "bubble": 6,
        "groups": ((0x319CB306, 0x80BD7F87), (0xE2E83849, 0x80BD80C0), (0xA4DFFD0C, 0x80BD8133)),
        "stages": (
            ((0xA4DFFD0C, tuple(range(1, 6))),), ((0xA4DFFD0C, (6, 7, 8)),),
            ((0xE2E83849, tuple(range(1, 9))),), ((0x319CB306, tuple(range(4, 9))),),
            ((0x319CB306, (2,)),),
        ), "boss": (0x319CB306, 2),
    },
    {
        "namespace": "io", "activity": "eden_freeroam", "scenario": 0x80B56B1B,
        "name": "Grove of Ulan-Tan", "bubble": 18,
        "groups": ((0xDF80EDAD, 0x80BD99FF), (0xCEA81A19, 0x80BD9A50),
                   (0x33C30847, 0x80BD9B3C), (0x87678E81, 0x80BD9B57)),
        "stages": (
            ((0xDF80EDAD, (1, 2)),), ((0xCEA81A19, tuple(range(1, 7))),),
            ((0x87678E81, tuple(range(1, 5))),), ((0x33C30847, (2, 5)),),
            ((0x33C30847, tuple(range(6, 12))),), ((0x33C30847, (12, 13, 3)),),
        ), "boss": (0x33C30847, 3),
    },
    {
        "namespace": "nessus", "activity": "planet_x_freeroam", "scenario": 0x80B43A1C,
        "name": "The Rift", "bubble": 5,
        "groups": ((0x6717656F, 0x80C06CF8), (0xD7BD9746, 0x80C06D74)),
        "stages": (
            ((0xD7BD9746, tuple(range(1, 7))),), ((0xD7BD9746, tuple(range(7, 11))),),
            ((0xD7BD9746, (11, 12)),), ((0x6717656F, (2, *range(3, 10))),),
            ((0x6717656F, tuple(range(10, 16))),), ((0x6717656F, (16, 17, 18, 0)),),
        ), "boss": (0x6717656F, 0),
    },
    {
        "namespace": "nessus", "activity": "planet_x_freeroam", "scenario": 0x80B43A1C,
        "name": "The Conflux", "bubble": 12,
        "groups": ((0xBB69D2E9, 0x80C08B57),),
        "stages": (
            ((0xBB69D2E9, (4, *range(5, 12))),), ((0xBB69D2E9, tuple(range(12, 16))),),
            ((0xBB69D2E9, tuple(range(16, 20))),), ((0xBB69D2E9, (20, 21, 2)),),
        ), "boss": (0xBB69D2E9, 2),
    },
    {
        "namespace": "nessus", "activity": "planet_x_freeroam", "scenario": 0x80B43A1C,
        "name": "The Orrery", "bubble": 14,
        "groups": ((0x3F8AF55C, 0x80C08ECC),),
        "stages": (
            ((0x3F8AF55C, (5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15)),),
            ((0x3F8AF55C, (16, 17, 18)),), ((0x3F8AF55C, tuple(range(19, 25))),),
            ((0x3F8AF55C, tuple(range(25, 30))),), ((0x3F8AF55C, (30, 31, 32, 2, 3)),),
        ), "boss": (0x3F8AF55C, 3),
    },
    {
        "namespace": "nessus", "activity": "planet_x_freeroam", "scenario": 0x80B43A1C,
        "name": "The Carrion Pit", "bubble": 31,
        "groups": ((0x719085A0, 0x80C09917), (0x2F8DB58A, 0x80C09958)),
        "stages": (
            ((0x719085A0, tuple(range(0, 6))),), ((0x719085A0, (6, 7, 8)),),
            ((0x2F8DB58A, (2, *range(3, 12))),), ((0x2F8DB58A, (12, 13, 14)),),
            ((0x2F8DB58A, (15, 16, 17, 18, 19, 0)),),
        ), "boss": (0x2F8DB58A, 0),
    },
    {
        "namespace": "nessus", "activity": "planet_x_freeroam", "scenario": 0x80B43A1C,
        "name": "Ancient's Haunt", "bubble": 39,
        "groups": ((0xE8346A52, 0x80C30C3C), (0x100F6578, 0x80C30CCD)),
        "stages": (
            ((0xE8346A52, tuple(range(0, 9))),), ((0xE8346A52, tuple(range(9, 15))),),
            ((0x100F6578, (2, *range(3, 8))),), ((0x100F6578, (8, 9, 0)),),
        ), "boss": (0x100F6578, 0),
    },
    {
        "namespace": "tangled_shore", "activity": "tangled_shore_freeroam", "scenario": 0x80FC9645,
        "name": "Trapper's Cave", "bubble": 8,
        "groups": ((0x51AA084F, 0x80FD5C92),
                   (0xB8D9E2F8, 0x80FD5CC6), (0xCED21737, 0x80FD5D19)),
        "stages": (
            ((0xB8D9E2F8, tuple(range(1, 7))),), ((0xCED21737, tuple(range(1, 7))),),
            ((0xCED21737, tuple(range(7, 12))),), ((0x51AA084F, (3, 4, 5, 1)),),
        ), "boss": (0x51AA084F, 1),
    },
    {
        "namespace": "tangled_shore", "activity": "tangled_shore_freeroam", "scenario": 0x80FC9645,
        "name": "Wolfship Turbine", "bubble": 10,
        "groups": ((0xAD5D1D0F, 0x80FD8581), (0xD8755C9C, 0x80FD85BF)),
        "stages": (
            ((0xAD5D1D0F, tuple(range(1, 8))),), ((0xD8755C9C, tuple(range(3, 11))),),
            ((0xD8755C9C, (1,)),),
        ), "boss": (0xD8755C9C, 1),
    },
    {
        "namespace": "tangled_shore", "activity": "tangled_shore_freeroam", "scenario": 0x80FC9645,
        "name": "Kingship Dock", "bubble": 15,
        "groups": ((0xED59DF6F, 0x80FD9694),),
        "stages": (((0xED59DF6F, tuple(range(1, 12))),), ((0xED59DF6F, (12,)),)),
        "boss": (0xED59DF6F, 12),
    },
    {
        "namespace": "tangled_shore", "activity": "tangled_shore_freeroam", "scenario": 0x80FC9645,
        "name": "The Empty Tank", "bubble": 16,
        "groups": ((0x9A24C39A, 0x80FD993B),),
        "stages": (
            ((0x9A24C39A, (2, 4, 6, 8, 10, 12, 14, 16, 17, 18, 19, 20, 21, 22, 37, 39, 42, 44)),),
            ((0x9A24C39A, (52, 56, 58, 60, 64, 70, 74, 76)),),
            ((0x9A24C39A, (84, 85, 86, 89, 92, 93, 94, 95, 96, 97, 98, 99, 101, 103, 82)),),
        ), "boss": (0x9A24C39A, 82),
    },
    {
        "namespace": "tangled_shore", "activity": "tangled_shore_freeroam", "scenario": 0x80FC9645,
        "name": "Shipyard AWO-43", "bubble": 19,
        "groups": ((0x5B603F1E, 0x80FDC342), (0x76A432A7, 0x80FDC3A6), (0xBD63AFEC, 0x80FDC3E4)),
        "stages": (
            ((0xBD63AFEC, tuple(range(1, 8))),), ((0x5B603F1E, (1, 2, 3, 4, 5, 7)),),
            ((0x76A432A7, tuple(range(1, 9))),),
        ), "boss": (0x76A432A7, 8),
    },
    {
        "namespace": "dreaming_city", "activity": "dreaming_city_freeroam", "scenario": 0x80F1404D,
        "name": "Bay of Drowned Wishes", "bubble": 8,
        "groups": ((0xB846777D, 0x80F25045),),
        "stages": (((0xB846777D, (*range(125, 142), 144, 146, 147, 149)),),),
        "boss": (0xB846777D, 144),
    },
    {
        "namespace": "dreaming_city", "activity": "dreaming_city_freeroam", "scenario": 0x80F1404D,
        "name": "Chamber of Starlight", "bubble": 9,
        "groups": ((0x74E16154, 0x80F25324),),
        "stages": (((0x74E16154, (0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12)),),),
        "boss": (0x74E16154, 0),
    },
    {
        "namespace": "dreaming_city", "activity": "dreaming_city_freeroam", "scenario": 0x80F1404D,
        "name": "Aphelion's Rest", "bubble": 10,
        "groups": ((0xD7FF797A, 0x80F255C4),),
        "stages": (((0xD7FF797A, (*range(1, 14), *range(15, 19))),),),
        "boss": (0xD7FF797A, 13),
    },
)

# Exact completion-placement groups in each Lost Sector bubble. These are
# separate from the encounter groups above. Their type-4 descriptors retain a
# package-authored chest role string and are admitted only for the boss-cleared
# completion projection.
REWARDS = {
    "Pariah's Refuge": (0x3F0E0EE6, 0x80F5E876, 0, 0x80F5E866, "dungeon_chest"),
    "Core Terminus": (0x7CF44AF0, 0x80F73C83, 1, 0x80F73CA9, "region_chest_major1"),
    "Ma'adim Subterrane": (0x662D2585, 0x80F76089, 0, 0x80F76103, "region_chest_major1"),
    "Methane Flush": (0x89BDCDA2, 0x80BF080B, 0, 0x80BF0489, "region_chest_major1"),
    "DS Quarters-2": (0x8A6203C8, 0x80BF65DF, 0, 0x80BF155A, "region_chest_major1"),
    "Cargo Bay 3": (0x32C796F1, 0x80BF6B83, 0, 0x80BF1827, "region_chest_major2"),
    "Sanctum of Bones": (0x4B422BBB, 0x80BD7F6D, 0, 0x80BD74EA, "region_chest_major1"),
    "Aphix Conduit": (0xB81A4C9E, 0x80BD874D, 0, 0x80BD78BB, "region_chest_major2"),
    "Grove of Ulan-Tan": (0x1FC9AD3A, 0x80C36427, 0, 0x80BD9089, "region_chest_major1"),
    "The Rift": (0x9F123D29, 0x80C06D92, 0, 0x80C07865, "region_chest_major1"),
    "The Conflux": (0x8ACACC33, 0x80C08BFA, 0, 0x80C08E8C, "region_chest_major1"),
    "The Orrery": (0x5D0FE7E1, 0x80C0902F, 0, 0x80C095E2, "region_chest_major1"),
    "The Carrion Pit": (0x8A55AD63, 0x80C09CC1, 0, 0x80C0A70B, "region_chest_major1"),
    "Ancient's Haunt": (0x26928E05, 0x80C31050, 0, 0x80C31BB3, "region_chest_major1"),
    "Trapper's Cave": (0x16972B37, 0x80FD5D26, 1, 0x80FD595C, "region_chest_major1"),
    "Wolfship Turbine": (0xAA9A0D8D, 0x80FD85CC, 1, 0x80FD8021, "region_chest_major1"),
    "Kingship Dock": (0xE0E3C57F, 0x80FD96A1, 0, 0x80FD9111, "region_chest_major1"),
    "The Empty Tank": (0x47A6AC0A, 0x80FD9948, 1, 0x80FD962A, "treasure_chest_dungeon"),
    "Shipyard AWO-43": (0xCA95C18E, 0x80FDC3F1, 0, 0x80FDC072, "o_dungeon_chest"),
    "Bay of Drowned Wishes": (0xBA29E558, 0x80F25086, 1, 0x80F24E41, "region_chest_major1"),
    "Chamber of Starlight": (0x039307C3, 0x80F25376, 1, 0x80F2524C, "region_chest_major1"),
    "Aphelion's Rest": (0xBA29E55E, 0x80F255FC, 1, 0x80F254A2, "region_chest_major1"),
}


def cpp(value: int) -> str:
    return f"0x{value:08X}U"


def digest(tag: int) -> str:
    _, raw = gen.package_read.read(tag)
    return hashlib.sha256(raw).hexdigest()


def tactical(group: dict, source: int) -> int:
    key = group["key"]
    if key in (0x3A80D8D4, 0x16833C32, 0x4F05045F):
        return 0
    if key == 0xD893B268:
        return 1 if source == 2 else 0
    if key == 0x7BFEF9E6:
        if source == 0:
            return 19
        if source == 2:
            return 22
        if source in (5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16):
            return 20
        return 21
    if key == 0x8DA9816E:
        return 13
    if key == 0x7E21B2C3:
        return 1 if source == 3 else 0
    if key == 0xA27443E8:
        return 0 if source == 3 else 1
    if key == 0x98B63236:
        return 13
    if key == 0x2E3D2EB4:
        if source == 0:
            return 20
        if source == 3:
            return 23
        return 21 if source in range(6, 18) else 22
    if key == 0x5BA616DA:
        return 0 if source == 21 else 1
    if key in (0x265E16C7, 0xA4DFFD0C, 0xE2E83849, 0xDF80EDAD,
               0xCEA81A19, 0x87678E81):
        return 0
    if key in (0xDB5D8740, 0x319CB306, 0x33C30847):
        boss = {0xDB5D8740: 2, 0x319CB306: 2, 0x33C30847: 3}[key]
        return 1 if source == boss else 0
    if key in (0xD7BD9746, 0xBB69D2E9, 0xE8346A52, 0x719085A0):
        return {0xD7BD9746: 0, 0xBB69D2E9: 1, 0xE8346A52: 15, 0x719085A0: 9}[key]
    if key == 0x6717656F:
        return 19 if source == 0 else 20
    if key == 0x3F8AF55C:
        return 0 if source == 3 else 1
    if key == 0x2F8DB58A:
        return 21 if source == 0 else 20
    if key == 0x100F6578:
        return 10 if source == 0 else 11
    if key in (0x51AA084F, 0xB8D9E2F8, 0xCED21737, 0xAD5D1D0F,
               0xD8755C9C, 0xED59DF6F, 0x5B603F1E, 0x76A432A7, 0xBD63AFEC):
        return 0
    if key == 0x9A24C39A:
        return 1 if source <= 44 else (-1 if source < 80 else 80)
    if key == 0xB846777D:
        return 148 if source in (144, 146, 147, 149) else 142
    if key == 0x74E16154:
        return 52
    if key == 0xD7FF797A:
        return 61
    raise ValueError(f"Lost Sector tactical policy missing for {key:08X}:{source}")


def source_rule(group: dict, source: int) -> tuple[int, str]:
    if group["key"] == 0x74E16154 and source in (11, 12):
        return 0, "implicit_fixed_turret"
    roles = ("fallback",) if group["key"] in (0x3A80D8D4, 0xD893B268) else ("primary", "fallback")
    for role in roles:
        try:
            return gen.source_spawn_rule(group, source, role), role
        except ValueError:
            pass
    raise ValueError(f"Lost Sector source has no reviewed rule {group['key']:08X}:{source}")


def quota_content_sha256(document: dict) -> str:
    canonical = json.dumps(document, ensure_ascii=False, sort_keys=True,
                           separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(canonical).hexdigest()


def load_encounter_quotas(path: Path = QUOTAS_PATH) -> tuple[dict, str]:
    raw = path.read_bytes()
    return json.loads(raw), hashlib.sha256(raw).hexdigest()


def load_footage_review(path: Path = FOOTAGE_REVIEW_PATH) -> tuple[dict, str]:
    raw = path.read_bytes()
    document = json.loads(raw)
    reviewed_names = [sector.get("name") for sector in document.get("sectors", [])]
    expected_names = [sector["name"] for sector in SECTORS]
    if document.get("schema") != 1 or len(reviewed_names) != len(set(reviewed_names)) \
            or set(reviewed_names) != set(expected_names):
        raise ValueError("Lost Sector footage-review coverage is incomplete or stale")
    return document, hashlib.sha256(raw).hexdigest()


def _validate_review(review: object, required: tuple[str, ...], context: str) -> None:
    if not isinstance(review, dict) or any(key not in review for key in required):
        raise ValueError(f"Lost Sector quota review fields missing for {context}")
    if not isinstance(review["status"], str) or not review["status"].strip():
        raise ValueError(f"Lost Sector quota review status invalid for {context}")
    if not isinstance(review["note"], str) or not review["note"].strip():
        raise ValueError(f"Lost Sector quota review note invalid for {context}")
    for key in ("reference", "video_or_guide_reference"):
        if key in required and review[key] != "tools/coo/lost_sector_footage_review.json":
            raise ValueError(f"Lost Sector quota review reference invalid for {context}")
    minimum = review.get("observed_minimum")
    if minimum is not None and (type(minimum) is not int or minimum < 0):
        raise ValueError(f"Lost Sector quota observed minimum invalid for {context}")
    timestamp = review.get("timestamp")
    if timestamp is not None and (not isinstance(timestamp, str) or not timestamp.strip()):
        raise ValueError(f"Lost Sector quota review timestamp invalid for {context}")


def validate_encounter_quotas(document: dict, groups: dict[tuple[int, int], dict]) \
        -> dict[tuple[str, int, int], dict]:
    """Validate the data-only quota layer against package-resolved source topology."""
    if not isinstance(document, dict) or document.get("schema") != 1:
        raise ValueError("Lost Sector quota schema changed")
    disclaimer = document.get("estimate_disclaimer")
    if not isinstance(disclaimer, str) or "estimate" not in disclaimer.lower() \
            or "retail" not in disclaimer.lower():
        raise ValueError("Lost Sector quota estimate disclaimer missing")
    if document.get("target_bounds") != {
        "minimum": QUOTA_TARGET_MINIMUM,
        "maximum": QUOTA_TARGET_MAXIMUM,
        "primary_must_be_positive": True,
        "boss_total_must_equal": 1,
    }:
        raise ValueError("Lost Sector quota target bounds changed")

    quota_sectors = document.get("sectors")
    if not isinstance(quota_sectors, dict):
        raise ValueError("Lost Sector quota sectors missing")
    expected_names = [sector["name"] for sector in SECTORS]
    if len(expected_names) != len(set(expected_names)) or set(quota_sectors) != set(expected_names):
        raise ValueError("Lost Sector quota sector coverage is incomplete or stale")

    rows: dict[tuple[str, int, int], dict] = {}
    for sector in SECTORS:
        name = sector["name"]
        quota_sector = quota_sectors[name]
        if not isinstance(quota_sector, dict) or (
            quota_sector.get("namespace") != sector["namespace"]
            or quota_sector.get("scenario") != f"{sector['scenario']:08X}"
            or quota_sector.get("bubble") != sector["bubble"]
        ):
            raise ValueError(f"Lost Sector quota identity changed for {name}")
        _validate_review(quota_sector.get("review"),
                         ("status", "video_or_guide_reference", "note"), name)
        quota_stages = quota_sector.get("stages")
        if not isinstance(quota_stages, list) or len(quota_stages) != len(sector["stages"]):
            raise ValueError(f"Lost Sector quota stage coverage changed for {name}")

        boss_hits = 0
        for stage_index, stage_spec in enumerate(sector["stages"]):
            quota_stage = quota_stages[stage_index]
            if not isinstance(quota_stage, dict) or quota_stage.get("stage") != stage_index:
                raise ValueError(f"Lost Sector quota stage identity changed for {name}:{stage_index}")
            quota_sources = quota_stage.get("sources")
            expected_sources = [(key, source) for key, sources in stage_spec for source in sources]
            if not isinstance(quota_sources, list) or [
                (row.get("registry"), row.get("source")) if isinstance(row, dict) else (None, None)
                for row in quota_sources
            ] != [(f"{key:08X}", source) for key, source in expected_sources]:
                raise ValueError(f"Lost Sector quota source coverage changed for {name}:{stage_index}")

            for quota_row, (key, source) in zip(quota_sources, expected_sources):
                row_key = (name, key, source)
                if row_key in rows:
                    raise ValueError(f"Lost Sector quota source repeated for {name}:{key:08X}:{source}")
                _validate_review(quota_row.get("review"),
                                 ("status", "reference", "observed_minimum", "timestamp", "note"),
                                 f"{name}:{key:08X}:{source}")
                group = groups[(sector["scenario"], key)]
                categories = gen.source_category_count(group, source)
                if categories < 1 or categories > 8:
                    raise ValueError(f"Lost Sector quota category unsupported for {name}:{key:08X}:{source}")
                targets = quota_row.get("targets")
                if not isinstance(targets, list) or len(targets) != categories:
                    raise ValueError(f"Lost Sector quota category arity changed for {name}:{key:08X}:{source}")
                if any(type(target) is not int or target < QUOTA_TARGET_MINIMUM
                       or target > QUOTA_TARGET_MAXIMUM for target in targets):
                    raise ValueError(f"Lost Sector quota target invalid for {name}:{key:08X}:{source}")
                if not targets or targets[0] <= 0:
                    raise ValueError(f"Lost Sector quota primary target invalid for {name}:{key:08X}:{source}")
                if sum(targets) > QUOTA_TARGET_MAXIMUM:
                    raise ValueError(f"Lost Sector quota exceeds runtime source limit for {name}:{key:08X}:{source}")
                if (key, source) != sector["boss"] and any(target == 0 for target in targets):
                    raise ValueError(f"Lost Sector quota disabled native category for {name}:{key:08X}:{source}")
                choices = gen.source_count_choices(group, source)
                for category_index, (target, category) in enumerate(zip(targets, choices)):
                    if target > 1 and any(
                        choice.get("rank") in counts.SINGLETON_RANKS
                        or choice.get("species") in counts.FIXED_SPECIES
                        for choice in category
                    ):
                        raise ValueError(
                            f"Lost Sector singleton quota multiplied for "
                            f"{name}:{key:08X}:{source}:{category_index}"
                        )
                if (key, source) == sector["boss"]:
                    boss_hits += 1
                    if targets != [1] + [0] * (categories - 1) or sum(targets) != 1:
                        raise ValueError(f"Lost Sector boss quota changed for {name}:{key:08X}:{source}")
                rows[row_key] = quota_row
        if boss_hits != 1:
            raise ValueError(f"Lost Sector quota boss coverage changed for {name}")
    return rows


def resolve() -> tuple[dict[tuple[int, int], dict], dict[int, list[int]]]:
    groups = {}
    hashes = {}
    for scenario in sorted({sector["scenario"] for sector in SECTORS}):
        bubble_hashes, objects = gen.scenario_objects(scenario)
        hashes[scenario] = bubble_hashes
        wanted = {(sector["bubble"], key): obj for sector in SECTORS if sector["scenario"] == scenario
                  for key, obj in (*sector["groups"], REWARDS[sector["name"]][:2])}
        for bubble, tags in objects.items():
            for tag in tags:
                group = gen.resolve_group(tag)
                if group and (bubble, group["key"]) in wanted:
                    if group["object"] != wanted[(bubble, group["key"])] or group["mask"] != 1 << bubble:
                        raise ValueError("Lost Sector registry identity changed")
                    groups[(scenario, group["key"])] = group
        if any((scenario, key) not in groups for bubble, key in wanted):
            raise ValueError("Lost Sector registry missing")
    return groups, hashes


def emit(quota_document: dict | None = None, quota_file_sha256: str | None = None) -> tuple[str, dict]:
    groups, hashes = resolve()
    if quota_document is None:
        quota_document, quota_file_sha256 = load_encounter_quotas()
    elif quota_file_sha256 is None:
        quota_file_sha256 = quota_content_sha256(quota_document)
    quota_rows = validate_encounter_quotas(quota_document, groups)
    footage_review, footage_review_sha256 = load_footage_review()
    by_namespace: dict[str, list[dict]] = {}
    for sector in SECTORS:
        by_namespace.setdefault(sector["namespace"], []).append(sector)
    lines = [
        "#pragma once", "", "#include \"lost_sector_runtime.h\"", "#include <array>", "",
        "namespace dawn::server::runtime::activity::lost_sector::catalog {", "",
        "template<class T,std::size_t A,std::size_t B>",
        "[[nodiscard]] consteval std::array<T,A+B> concat(const std::array<T,A>& first,const std::array<T,B>& second) {",
        "    std::array<T,A+B> result{};std::size_t out{};",
        "    for(const auto& value:first)result[out++]=value;",
        "    for(const auto& value:second)result[out++]=value;",
        "    return result;",
        "}", "",
        "// Generated from installed build 86657. Source/rule/point ownership is native;",
        "// stage order and initial tactical row zero are a reviewed host reconstruction.",
    ]
    evidence = {
        "schema": 3, "package_build": 86657, "sectors": [], "registry_proofs": [],
        "quota_manifest": {
            "path": "tools/coo/lost_sector_encounter_quotas.json",
            "schema": quota_document["schema"],
            "file_sha256": quota_file_sha256,
            "content_sha256": quota_content_sha256(quota_document),
            "estimate_disclaimer": quota_document["estimate_disclaimer"],
        },
        "footage_review": {
            "path": "tools/coo/lost_sector_footage_review.json",
            "file_sha256": footage_review_sha256,
            "method": footage_review["method"],
        },
        "omitted_sources": [
            {"registry": f"{key:08X}", "source": source, "reason": reason}
            for (key, source), reason in sorted(OMITTED_SOURCES.items())
        ],
        "limitations": [
            "The Empty Tank type-4 boss-shield placement 106 and entrances 104/105 are not activated; "
            "this encounter/boss pass does not claim to reproduce the retail shield or door phase."
        ],
    }
    for namespace, sectors in by_namespace.items():
        unique = []
        for sector in sectors:
            for key, _ in sector["groups"]:
                group = groups[(sector["scenario"], key)]
                if group not in unique:
                    unique.append(group)
        rewards = []
        for sector in sectors:
            key = REWARDS[sector["name"]][0]
            group = groups[(sector["scenario"], key)]
            if group not in rewards:
                rewards.append(group)
        all_groups = unique + rewards
        registry_index = {group["key"]: index for index, group in enumerate(unique)}
        reward_index = {group["key"]: index for index, group in enumerate(rewards)}
        lines.extend(["", f"namespace {namespace} {{"])
        for index, group in enumerate(all_groups):
            lines.append(f"inline constexpr std::array<registry::Slot,{len(group['slots'])}> kSlots{index}{{{{")
            for slot in group["slots"]:
                lines.append(f"    {{{slot['index']},{slot['type']},{cpp(slot['component'])},{cpp(slot['sense'])},"
                             f"{cpp(slot['authority'])},{cpp(slot['descriptor'])}}},")
            lines.append("}};")
        lines.append(f"inline constexpr std::array<registry::Definition,{len(unique)}> kRegistries{{{{")
        for index, group in enumerate(unique):
            sector = next(s for s in sectors if any(key == group["key"]
                for key, _ in s["groups"]))
            lines.append(f"    {{\"{sector['activity']}\",{cpp(sector['scenario'])},{cpp(group['key'])},"
                         f"{cpp(group['object'])},{cpp(hashes[sector['scenario']][sector['bubble']])},"
                         f"{sector['bubble']},kSlots{index}}},")
        lines.append("}};")
        lines.append(f"inline constexpr std::array<registry::Definition,{len(rewards)}> kRewardRegistries{{{{")
        for index, group in enumerate(rewards):
            sector = next(s for s in sectors if REWARDS[s["name"]][0] == group["key"])
            lines.append(f"    {{\"{sector['activity']}\",{cpp(sector['scenario'])},{cpp(group['key'])},"
                         f"{cpp(group['object'])},{cpp(hashes[sector['scenario']][sector['bubble']])},"
                         f"{sector['bubble']},kSlots{len(unique)+index}}},")
        lines.append("}};")
        capabilities = []
        policies = []
        stages = []
        sector_rows = []
        for sector in sectors:
            first_stage = len(stages)
            quota_sector = quota_document["sectors"][sector["name"]]
            sector_evidence = {"name": sector["name"], "scenario": f"{sector['scenario']:08X}",
                               "bubble": sector["bubble"], "quota_review": quota_sector["review"],
                               "stages": []}
            for stage_spec in sector["stages"]:
                first = len(capabilities)
                stage_evidence = []
                for key, source_slots in stage_spec:
                    group = groups[(sector["scenario"], key)]
                    for source in source_slots:
                        source_slot = next(x for x in group["slots"] if x["type"] == 1 and x["index"] == source)
                        rule, rule_role = source_rule(group, source)
                        rule_slot = next((x for x in group["slots"]
                                          if x["type"] == 66 and x["index"] == rule), None)
                        if rule_role != "implicit_fixed_turret" and rule_slot is None:
                            raise ValueError("Lost Sector selected rule slot missing")
                        objective = tactical(group, source)
                        objective_slot = next((x for x in group["slots"] if x["type"] == 3 and x["index"] == objective), None)
                        if objective_slot is None and (key, source) not in {
                            (0x9A24C39A, 52), (0x9A24C39A, 56), (0x9A24C39A, 58),
                            (0x9A24C39A, 60), (0x9A24C39A, 64), (0x9A24C39A, 70),
                            (0x9A24C39A, 74), (0x9A24C39A, 76)
                        }:
                            raise ValueError("Lost Sector tactical objective missing without reviewed no-tactical policy")
                        categories = gen.source_category_count(group, source)
                        choices = gen.source_count_choices(group, source)
                        boss = (key, source) == sector["boss"]
                        quota_row = quota_rows[(sector["name"], key, source)]
                        targets = quota_row["targets"]
                        row_count = gen.tactical_row_count(group, objective) if objective_slot else 0
                        mask = (1 << row_count) - 1 if row_count else 0
                        capabilities.append((registry_index[key], source, rule, objective, mask, categories,
                                             rule_role != "implicit_fixed_turret"))
                        policies.append((targets, boss))
                        stage_evidence.append({
                            "registry": f"{key:08X}", "source": source,
                            "source_descriptor": f'{source_slot["descriptor"]:08X}',
                            "source_sha256": digest(source_slot["descriptor"]),
                            "rule_role": rule_role, "rule": rule,
                            "rule_descriptor": f'{rule_slot["descriptor"]:08X}' if rule_slot else None,
                            "rule_sha256": digest(rule_slot["descriptor"]) if rule_slot else None,
                            "tactical": objective, "tactical_row": 0, "tactical_rows": row_count,
                            "tactical_descriptor": f'{objective_slot["descriptor"]:08X}' if objective_slot else None,
                            "tactical_sha256": digest(objective_slot["descriptor"]) if objective_slot else None,
                            "categories": categories, "targets": targets,
                            "quota_review": quota_row["review"],
                            "count_choices_sha256": gen.count_choices_digest(choices), "boss": boss,
                        })
                stages.append((first, len(capabilities) - first))
                sector_evidence["stages"].append(stage_evidence)
            reward_key, _, reward_slot, reward_descriptor, reward_role = REWARDS[sector["name"]]
            reward_group = groups[(sector["scenario"], reward_key)]
            reward = next((slot for slot in reward_group["slots"]
                if slot["index"] == reward_slot and slot["type"] == 4), None)
            if reward is None or reward["descriptor"] != reward_descriptor:
                raise ValueError(f"Lost Sector chest placement changed for {sector['name']}")
            _, reward_raw = gen.package_read.read(reward_descriptor)
            if reward_role.encode("ascii") not in reward_raw:
                raise ValueError(f"Lost Sector chest role changed for {sector['name']}")
            sector_evidence["reward"] = {
                "registry": f"{reward_key:08X}", "object": f"{reward_group['object']:08X}",
                "slot": reward_slot, "descriptor": f"{reward_descriptor:08X}",
                "role": reward_role, "descriptor_sha256": digest(reward_descriptor),
            }
            sector_rows.append((sector["name"], sector["bubble"], first_stage,
                                len(stages) - first_stage, reward_index[reward_key], reward_slot))
            evidence["sectors"].append(sector_evidence)
        for registry_id, group in enumerate(unique):
            enabled = {source for rid, source, *_ in capabilities if rid == registry_id}
            if not enabled:
                reward_sector = next((sector for sector in sectors
                    if REWARDS[sector["name"]][0] == group["key"]), None)
                if reward_sector is None:
                    raise ValueError("runtime Lost Sector registry has no enabled source or chest")
                continue
            raw_sources = {slot["index"] for slot in group["slots"] if slot["type"] == 1}
            expected_omitted = {source for key, source in OMITTED_SOURCES if key == group["key"]}
            if raw_sources - enabled != expected_omitted or raw_sources != enabled | expected_omitted:
                raise ValueError("Lost Sector source omission policy is incomplete or stale")
            reviewed_group = dict(group)
            reviewed_group["slots"] = [slot for slot in group["slots"]
                                         if slot["type"] != 1 or (slot["index"] in enabled
                                            and not (group["key"] == 0x74E16154 and slot["index"] in (11, 12)))]
            selected_rules = {source: rule for rid, source, rule, *_, has_rule in capabilities
                              if rid == registry_id and has_rule}
            point_lists, point_containers = POINT_DEPENDENCIES[group["key"]]
            scenario = next(sector["scenario"] for sector in sectors
                            if any(key == group["key"] for key, _ in sector["groups"]))
            proof = cohorts.native_evidence(gen, SimpleNamespace(scenario=scenario), reviewed_group,
                                            point_lists, point_containers,
                                            evidence_mode="selected_rules", selected_rules=selected_rules)
            evidence["registry_proofs"].append({
                "scenario": f"{scenario:08X}", "registry": f"{group['key']:08X}",
                "object": f"{group['object']:08X}", "enabled_sources": sorted(enabled),
                "omitted_sources": sorted(slot["index"] for slot in group["slots"]
                                          if slot["type"] == 1 and slot["index"] not in enabled),
                "point_lists": point_lists, "point_containers": point_containers,
                "native_point_closure_sha256": proof,
                "implicit_ruleless_sources": [
                    {"source": slot["index"], "descriptor": f'{slot["descriptor"]:08X}',
                     "descriptor_sha256": digest(slot["descriptor"])}
                    for slot in group["slots"] if slot["type"] == 1 and slot["index"] in enabled
                    and slot["index"] not in selected_rules
                ],
            })
        lines.append(f"inline constexpr std::array<population::Capability,{len(capabilities)}> kCapabilities{{{{")
        for registry_id, source, rule, objective, mask, categories, has_rule in capabilities:
            key = unique[registry_id]["key"]
            tactical_cpp = f"{{{cpp(key)},{objective},0}}" if objective >= 0 else "{}"
            lines.append(f"    {{&kRegistries[{registry_id}],{source},{rule},{tactical_cpp},"
                         f"{'true' if has_rule else 'false'},{cpp(mask)},false,population::kNoNamedMember,{categories}}},")
        lines.append("}};")
        lines.append(f"inline constexpr std::array<SourcePolicy,{len(policies)}> kPolicies{{{{")
        for targets, boss in policies:
            first=targets[0]
            second=targets[1] if len(targets)>1 else 0
            additional=targets[2:]+[0]*(8-len(targets))
            lines.append(f"    {{{first},{second},{'true' if boss else 'false'},"
                         f"{{{','.join(str(value) for value in additional[:6])}}},{len(targets)}}},")
        lines.append("}};")
        lines.append(f"inline constexpr std::array<Stage,{len(stages)}> kStages{{{{")
        for first, count in stages:
            lines.append(f"    {{{first},{count}}},")
        lines.append("}};")
        lines.append(f"inline constexpr std::array<Sector,{len(sector_rows)}> kSectors{{{{")
        for name, bubble, first, count, reward_registry, reward_slot in sector_rows:
            lines.append(f"    {{\"{name}\",{bubble},{first},{count},&kRewardRegistries[{reward_registry}],{reward_slot}}},")
        lines.extend(["}};", "[[nodiscard]] constexpr Definition definition(std::uint16_t capabilityBase) noexcept {",
                      "    return {kSectors,kStages,kPolicies,capabilityBase};", "}", f"}} // namespace {namespace}"])
    lines.extend(["", "namespace none {",
                  "inline constexpr std::array<registry::Definition,0> kRegistries{};",
                  "inline constexpr std::array<registry::Definition,0> kRewardRegistries{};",
                  "inline constexpr std::array<population::Capability,0> kCapabilities{};",
                  "inline constexpr std::array<SourcePolicy,0> kPolicies{};",
                  "inline constexpr std::array<Stage,0> kStages{};",
                  "inline constexpr std::array<Sector,0> kSectors{};",
                  "[[nodiscard]] constexpr Definition definition(std::uint16_t capabilityBase) noexcept {",
                  "    return {kSectors,kStages,kPolicies,capabilityBase};",
                  "}",
                  "}", "", "} // namespace dawn::server::runtime::activity::lost_sector::catalog", ""])
    return "\n".join(lines), evidence


def main() -> None:
    header, evidence = emit()
    output = ROOT / "Dawn/src/server/runtime/activity/lost_sector_catalog.h"
    output.write_text(header, encoding="utf-8", newline="\r\n")
    evidence_output = ROOT / "tools/coo/lost_sector_mercury_mars_evidence.json"
    evidence_output.write_text(json.dumps(evidence, indent=2) + "\n")
    print(f"generated {output.relative_to(ROOT)} and {evidence_output.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
