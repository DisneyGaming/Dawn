"""Decode Mercury's installed patrol catalog and chest-skirmish source choices.

These are package facts. Candidate weights, cooldown units, host selection,
source request counts and progression are deliberately not synthesized here.
The caller supplies an authenticated, class-checking package read function.
"""
import struct


def u32(data, offset):
    return struct.unpack_from('<I', data, offset)[0]


def relative(data, offset, size=0):
    target = offset + struct.unpack_from('<q', data, offset)[0]
    if not 4 <= target <= len(data) - size:
        raise ValueError('relative pointer bounds')
    return target


def rows(data, offset, expected, stride):
    count = struct.unpack_from('<Q', data, offset)[0]
    if not 0 < count <= 300000:
        raise ValueError('array count')
    header = relative(data, offset + 8, 16)
    if u32(data, header - 4) != 0x80809FBD or u32(data, header + 8) != expected:
        raise ValueError('array class')
    if struct.unpack_from('<Q', data, header)[0] != count:
        raise ValueError('array count mismatch')
    first = header + 16
    if first + count * stride > len(data):
        raise ValueError('array element bounds')
    return [first + index * stride for index in range(count)]


def catalog(data):
    result = []
    for index, row in enumerate(rows(data, 8, 0x80809442, 96)):
        label_at = relative(data, row, 1)
        label_end = data.find(b'\0', label_at)
        if not label_at < label_end <= label_at + 160:
            raise ValueError('catalog label bounds')
        candidates = []
        for candidate in rows(data, row + 56, 0x80809444, 40):
            candidates.append({
                'offset': candidate,
                'scalar_00': struct.unpack_from('<f', data, candidate)[0],
                'word_0c': f'{u32(data, candidate + 12):08X}',
                'bubble_hash': f'{u32(data, candidate + 16):08X}',
                'registry': f'{u32(data, candidate + 20):08X}',
                'word_18': f'{u32(data, candidate + 24):08X}',
                'word_1c': f'{u32(data, candidate + 28):08X}',
                'rally_registry': f'{u32(data, candidate + 32):08X}',
                'word_24': f'{u32(data, candidate + 36):08X}',
            })
        result.append({'row': index, 'offset': row,
                       'label': data[label_at:label_end].decode('ascii'),
                       'group_hash': f'{u32(data, row + 8):08X}',
                       'scalars_24_28_2c_30': list(struct.unpack_from('<4f', data, row + 36)),
                       'candidates': candidates})
    return result


def source_choices(data):
    definition = relative(data, 24, 0xB8)
    if u32(data, definition - 4) != 0x8080948F:
        raise ValueError('source definition class')
    result = []
    for category in rows(data, definition + 0xA8, 0x80808356, 104):
        variants = []
        for variant in range(6):
            choices = []
            for choice in rows(data, category + 8 + variant * 16, 0x80808358, 24):
                entity = relative(data, choice, 0x90)
                if u32(data, entity - 4) != 0x808099D8:
                    raise ValueError('source choice entity class')
                # Authored selectors are reached through the transform's typed
                # payload, optionally wrapped in a named combatant member.
                selector = relative(data, entity + 0x78, 32)
                wrapper_class = u32(data, selector - 4)
                named_member = wrapper_class == 0x80807EB6
                # Tower-back source80F5B6C8 has another typed wrapper. Its
                # self-relative first member explicitly points to808038A0;
                # the remaining wrapper fields are not interpreted here.
                if wrapper_class in (0x80807EB6, 0x80804B8B):
                    selector = relative(data, selector, 32)
                if u32(data, selector - 4) != 0x808038A0:
                    raise ValueError('source choice selector class')
                attributes = [
                    {'key': f'{u32(data, attribute):08X}',
                     'value': f'{u32(data, attribute + 4):08X}'}
                    for attribute in rows(data, selector + 16, 0x8080389F, 8)]
                choices.append({'entity': f'{u32(data, entity):08X}',
                                'weight': u32(data, choice + 12),
                                'named_member': named_member,
                                'selector_wrapper': f'{wrapper_class:08X}',
                                'selector_attributes': attributes})
            variants.append(choices)
        result.append({'key': f'{u32(data, category):08X}', 'variants': variants,
                       'all_six_choices_identical': all(v == variants[0] for v in variants)})
    return {'definition_offset': definition, 'categories': result,
            'requested_actors': None, 'host_variant_selection': None}


def tactical_rows(data):
    """Follow the typed tactical body, group/task arrays and scoped task refs."""
    definition = relative(data, 24, 0x98)
    if u32(data, definition - 4) != 0x8080835A:
        raise ValueError('tactical definition class')
    hashes = [u32(data, row) for row in rows(data, definition + 0x60, 0x80800070, 4)]
    result = []
    for index, row in enumerate(rows(data, definition + 0x88, 0x80807D8F, 40)):
        name_index = u32(data, row + 12)
        if name_index >= len(hashes):
            raise ValueError('tactical group hash index')
        tasks = []
        for task in rows(data, row + 16, 0x80807D95, 40):
            key, kind, slot = struct.unpack_from('<IBxH', data, task + 32)
            tasks.append({'registry': f'{key:08X}', 'type': kind, 'slot': slot})
        result.append({'row': index, 'hash_index': name_index,
                       'group_hash': f'{hashes[name_index]:08X}', 'tasks': tasks})
    return {'rows': result, 'source_to_row_assignment': None}


CATALOG_KEYS = (
    (0xC8229B2B,),
    (0x0F075D5A, 0x0F075D59, 0x0F075D58, 0x355A12CE, 0xE3209A7B, 0x2B982968, 0x6AF0D65D),
    (0x048FCC18,), (0x08FC5C1B,), (0xDFEBC768, 0xDFEBC76B), (0x048FCC1B,),
    (0x1BD03273, 0x1BD03270), (0x5B4B870A, 0x5B4B8709, 0x5B4B8708),
)

CHEST_SOURCES = (
    (0x80F5BFA1, 0x0F075D5A, ((0, 0x80F5BF3A), (2, 0x80F5BF50), (12, 0x80F5BF53)), 0x80F5BF93),
    (0x80F5BFF4, 0x0F075D59, ((0, 0x80F5BF9C), (2, 0x80F5BFA3), (12, 0x80F5BFA6)), 0x80F5BFE6),
    (0x80F5E047, 0x0F075D58, ((0, 0x80F5BFEF), (2, 0x80F5BFF6), (12, 0x80F5BFF9)), 0x80F5E039),
)


def extract(read, scenario, placed):
    if u32(scenario, 128) != 0x80F55240:
        raise ValueError('patrol catalog scenario link')
    entries = catalog(read(0x80F55240, 0x80809440))
    if tuple(tuple(int(c['registry'], 16) for c in r['candidates']) for r in entries) != CATALOG_KEYS:
        raise ValueError('patrol candidate set changed')
    if [r['group_hash'] for r in entries] != [
            '68D50CE5', '22D671D8', '22D671D8', '8120B6B5',
            'ED6C83B0', 'ED6C83B0', '8B1FC23D', '2E78284E']:
        raise ValueError('patrol group set changed')
    by_key = {}
    wanted = {key for row in CATALOG_KEYS for key in row}
    for tag in placed:
        body = read(tag, 0x80809462)
        key = u32(body, 12)
        if key in wanted:
            if key in by_key and by_key[key] != tag:
                raise ValueError('ambiguous patrol registry')
            by_key[key] = tag
    if set(by_key) != wanted:
        raise ValueError('patrol candidate ownership missing')
    for row in entries:
        for candidate in row['candidates']:
            candidate['object'] = f'{by_key[int(candidate["registry"], 16)]:08X}'
            if candidate['bubble_hash'] != 'A83A9175':
                raise ValueError('patrol candidate bubble changed')
    cohorts = []
    for obj, key, sources, tactical in CHEST_SOURCES:
        record = {'object': f'{obj:08X}', 'registry': f'{key:08X}', 'sources': []}
        for slot, descriptor in sources:
            redirect = read(descriptor, 0x80809B14)
            resource = u32(redirect, 12)
            body = read(resource, 0x80809C36)
            choices = source_choices(body)
            definition = choices['definition_offset']
            if body[definition + 48:definition + 56] != struct.pack('<IHH', key, 1, slot):
                raise ValueError('source scoped identity changed')
            record['sources'].append({'slot': slot, 'descriptor': f'{descriptor:08X}',
                                      'resource': f'{resource:08X}', **choices})
        redirect = read(tactical, 0x80809B14)
        resource = u32(redirect, 12)
        record['tactical'] = {'slot': 10, 'descriptor': f'{tactical:08X}',
                              'resource': f'{resource:08X}', **tactical_rows(read(resource, 0x80809C36))}
        cohorts.append(record)
    return {'scenario_field': 128, 'catalog': '80F55240', 'entries': entries,
            'catalog_scalar_semantics': None, 'chest_skirmish_sources': cohorts,
            'faction_war_controller': None, 'escalation_condition': None,
            'completion_condition': None, 'cooldown': None, 'activation_supported': False,
            'native_source_choice_consumer': '4EAE40',
            'native_tactical_consumers': ['4E2A90', '4EC710', 'AB5810', 'A9AD70', '4E2C40']}
