"""User-directed squad sizes, not a claim about recovered retail quotas.

Counts are deterministic per native source/category. Native weighted choices
remain alternatives, not one independently requested squad per choice.
"""
from __future__ import annotations

import hashlib


POLICY_ID = "user-species-counts-v1"
# (baseline, minimum, maximum). Only explicitly variable/non-singleton types
# vary. The fixed-one set is deliberately independent of the caller's default.
RULES = {
    "goblin": (3, 2, 4), "hobgoblin": (2, 1, 3), "harpy": (3, 2, 4),
    "supplicant": (3, 2, 4), "minotaur": (1, 1, 1), "hydra": (1, 1, 1),
    "cyclops": (1, 1, 1), "fanatic": (3, 2, 4),
    "dreg": (3, 2, 4), "wretch": (3, 2, 4), "vandal": (2, 1, 3),
    "marauder": (2, 1, 3), "captain": (1, 1, 1),
    "shank": (3, 2, 4), "shank_exploder": (2, 1, 3),
    "exploder_shank": (2, 1, 3), "tracer_shank": (2, 1, 3),
    "heavy_shank": (1, 1, 1), "servitor": (1, 1, 1), "walker": (1, 1, 1),
    "thrall": (6, 5, 7), "cursed_thrall": (2, 1, 3),
    "thrall_exploder": (2, 1, 3), "acolyte": (3, 2, 4),
    "knight": (1, 1, 2), "wizard": (1, 1, 1), "ogre": (1, 1, 1),
    "shrieker": (1, 1, 1),
    "legionary": (4, 4, 6), "phalanx": (2, 1, 3), "psion": (2, 1, 3),
    "centurion": (2, 1, 3), "colossus": (1, 1, 1), "incendior": (1, 1, 1),
    "war_beast": (3, 2, 4), "scorpius": (1, 1, 1),
    "stalker": (5, 4, 6), "raider": (3, 2, 4), "lurker": (3, 2, 4),
    "ravager": (2, 1, 3), "wraith": (2, 1, 3), "screeb": (5, 4, 6),
    "chieftain": (1, 1, 1), "abomination": (1, 1, 1),
    "shadow_thrall": (8, 7, 9), "acolytes_eye": (1, 1, 2), "chimera": (1, 1, 1),
}
# Major is also capped conservatively: its exact HUD segmentation is not
# established for every template, so bulk requests must not rely on its color.
SINGLETON_RANKS = frozenset(("major", "miniboss", "ultra", "boss"))
KNOWN_RANKS = SINGLETON_RANKS | {"minor"}
FIXED_SPECIES = frozenset(name for name, rule in RULES.items() if rule[1:] == (1, 1))
UNVERIFIED_ONE_DEFAULTS = frozenset(("knight", "acolytes_eye"))


def fnv1(text: str) -> int:
    value = 0x811C9DC5
    for byte in text.encode("ascii"):
        value = ((value * 16777619) ^ byte) & 0xFFFFFFFF
    return value


SPECIES_BY_HASH = {fnv1(name): name for name in RULES}
RANK_BY_HASH = {fnv1(name): name for name in KNOWN_RANKS}


def category_target(identity: str, choices: list[dict]) -> tuple[int, str]:
    """Return a safe shared request count for every possible native choice.

    Unknown and boss-capable categories stay at one; do not multiply a boss
    because a sibling alternative happens to be a common infantry template.
    Disjoint type ranges use the lower baseline as an explicitly conservative
    shared count. This is not a guarantee of each alternative's usual quota.
    """
    if not choices:
        return 1, "unresolved-empty-category"
    ranges = []
    for choice in choices:
        rank, species = choice.get("rank"), choice.get("species")
        if rank in SINGLETON_RANKS:
            return 1, "segmented-rank-singleton"
        if species in FIXED_SPECIES:
            return 1, "fixed-species-singleton"
        if species in UNVERIFIED_ONE_DEFAULTS:
            # The adopted reconstruction plan requires encounter evidence
            # before permitting two Knights or two Acolyte's Eyes.
            return 1, "encounter-count-unverified-singleton"
        if rank not in KNOWN_RANKS or species not in RULES:
            return 1, "unresolved-choice-singleton"
        ranges.append(RULES[species])
    low = max(rule[1] for rule in ranges)
    high = min(rule[2] for rule in ranges)
    if low > high:
        # One request vector cannot represent separate per-choice quotas.
        return min(rule[0] for rule in ranges), "mixed-choice-conservative"
    salt = int.from_bytes(hashlib.sha256((POLICY_ID + ":" + identity).encode("ascii")).digest()[:4], "little")
    return low + salt % (high - low + 1), "user-species-varied"
