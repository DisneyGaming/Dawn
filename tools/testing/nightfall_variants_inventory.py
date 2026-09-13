"""Verify curated Nightfalls against build 86657 packages and write a small evidence manifest.

Uses the existing local package reader. Does not modify the game, settings or packages.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def relative(data, offset):
    target = offset + struct.unpack_from('<q', data, offset)[0]
    if not 0 <= target < len(data):
        raise ValueError('Invalid relative pointer')
    return target


def fnv1(text):
    value = 0x811C9DC5
    for byte in text.encode('ascii'):
        value = ((value * 0x01000193) & 0xFFFFFFFF) ^ byte
    return value


PROPERTY_NAMES = {
    fnv1('difficulty'): 'difficulty',
    fnv1('activity_tier'): 'activity_tier',
    fnv1('starting_revive_tokens'): 'starting_revive_tokens',
    fnv1('resurrection_token_count'): 'resurrection_token_count',
    fnv1('time_limit'): 'time_limit',
    fnv1('ai_resistance_tier'): 'ai_resistance_tier',
    fnv1('ai_shield_tier'): 'ai_shield_tier',
}


def settings_properties(data):
    """Read the first value word from each polymorphic difficulty property."""
    count = struct.unpack_from('<Q', data, 8)[0]
    table = relative(data, 16) + 16
    if count > 64 or table + count * 8 > len(data):
        raise ValueError('Unexpected difficulty property table')
    properties = {}
    for index in range(count):
        target = relative(data, table + index * 8)
        if target < 4 or target + 8 > len(data):
            raise ValueError('Invalid difficulty property')
        name_hash = u32(data, target)
        if name_hash in properties:
            raise ValueError('Duplicate difficulty property')
        properties[name_hash] = (u32(data, target - 4), u32(data, target + 4))
    return properties


def selected_settings(properties):
    selected = {}
    for name_hash, name in PROPERTY_NAMES.items():
        if name_hash not in properties:
            continue
        property_class, value = properties[name_hash]
        selected[name] = {
            'hash': f'{name_hash:08X}',
            'class': f'{property_class:08X}',
            'value_word': f'{value:08X}',
        }
        if name == 'time_limit':
            selected[name]['seconds'] = struct.unpack('<f', struct.pack('<I', value))[0]
        else:
            selected[name]['value'] = value
    return selected


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(ROOT / 'tools/coo'))
    import package_read
    cls, public = package_read.read(0x81327CF0)
    count = struct.unpack_from('<Q', public, 8)[0]
    start = relative(public, 16) + 16
    if count != 1170 or u32(public, start - 8) != 0x808076FC:
        raise ValueError('Unexpected public activity table')
    header = (ROOT / 'Sunrise/src/state/activity/strike_variants.h').read_text()
    rows = re.findall(r'\{"(strike_\w+)", Difficulty::(\w+), (\d+), 0x([0-9A-F]+)U\}', header)
    if len(rows) != 8:
        raise ValueError('Expected two strikes with four variants each')
    expected = {'adept': (0x813206C1, 750), 'master': (0x81327CEB, 1080),
                'grandmaster': (0x81327CEF, 1100)}
    manifest = {'build': 86657, 'public_table': '81327CF0',
                'public_table_class': f'{cls:08X}',
                'public_table_sha256': hashlib.sha256(public).hexdigest(),
                'sunrise_grandmaster_power_policy': {
                    'native_activity_power': 1100,
                    'minimum_disadvantage': 30,
                    'default_player_power_cap': 1070,
                },
                'native_skull_selector_mapping_verified': False,
                'variants': []}
    for package, difficulty, ordinal, identity in rows:
        ordinal = int(ordinal)
        entry = start + ordinal * 16
        record = relative(public, entry + 8)
        name_at = relative(public, record + 0x68)
        native_name = public[name_at:public.index(0, name_at)].decode('ascii')
        if u32(public, entry) != int(identity, 16) or u32(public, record) != int(identity, 16) or native_name != package:
            raise ValueError(f'Activity identity mismatch: {ordinal}')
        settings_tag = u32(public, record + 0x98)
        row = {'package': package, 'difficulty': difficulty, 'activity': ordinal, 'hash': identity,
               'gameplay_settings_hash': f'{u32(public, record + 0xDC):08X}',
               'difficulty_settings_tag': f'{settings_tag:08X}',
               'public_record_offset': record, 'public_record_b4': u32(public, record + 0xB4),
               'modifier_count': struct.unpack_from('<Q', public, record + 0x88)[0]}
        if difficulty != 'standard':
            tag, power = expected[difficulty]
            if settings_tag != tag or row['public_record_b4'] != power or row['gameplay_settings_hash'] != '0B7A0305':
                raise ValueError(f'Wrong native difficulty settings: {ordinal}')
            settings_class, settings = package_read.read(tag)
            if settings_class != 0x80807D82:
                raise ValueError('Wrong difficulty settings class')
            row['difficulty_settings_sha256'] = hashlib.sha256(settings).hexdigest()
            properties = settings_properties(settings)
            row['difficulty_property_count'] = len(properties)
            row['selected_difficulty_properties'] = selected_settings(properties)

            expected_properties = {
                'adept': {0x11576EEA: 1, 0xF3BC4081: 0x202,
                          0x45701908: 0, 0x554EF48C: 0},
                'master': {0x11576EEA: 4, 0xF3BC4081: 0x204,
                           0x45701908: 3, 0x554EF48C: 2},
                'grandmaster': {0x11576EEA: 5, 0xF3BC4081: 0x204,
                                0x45701908: 3, 0x554EF48C: 2,
                                0x419EC930: 4, 0x42915C66: 0,
                                0xF5EC68CA: 0x4528C000},
            }[difficulty]
            if any(properties.get(key, (None, None))[1] != value
                   for key, value in expected_properties.items()):
                raise ValueError(f'Wrong native difficulty properties: {ordinal}')
        manifest['variants'].append(row)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf8')
    print('Verified all 8 native identities, power rows, and selected difficulty properties.')


if __name__ == '__main__':
    main()
