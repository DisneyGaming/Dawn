"""Strict package extraction for the faction incident's native HUD presentation.

Offsets refer to serialized tag bytes, including their eight-byte file header.
This module does not encode, send, hook, or acknowledge an incident.
"""
import struct

from mercury_faction_battle_catalog import relative, rows, u32


def incident_presentation(data, incident_hash):
    definitions = rows(data, 0x18, 0x80808782, 40)
    matches = [(index, at) for index, at in enumerate(definitions)
               if u32(data, at) == incident_hash]
    if len(matches) != 1:
        raise ValueError('incident identity is not unique')
    index, at = matches[0]
    # 4A94B0 consumes the signed 16-bit presentation list index at row+0x10.
    list_index = struct.unpack_from('<h', data, at + 0x10)[0]
    lists = rows(data, 0x38, 0x80808788, 16)
    if not 0 <= list_index < len(lists):
        raise ValueError('incident presentation list index')
    hashes = rows(data, lists[list_index], 0x8080878A, 4)
    payload = relative(data, at + 8, 8)
    return {'incident_row': index, 'incident_offset': at,
            'type': u32(data, at + 0x24),
            'presentation_list_index': list_index,
            'presentation_list_offset': lists[list_index],
            'presentation_hashes': [f'{u32(data, p):08X}' for p in hashes],
            'payload_class': f'{u32(data, payload - 4):08X}',
            'payload_offset': payload, 'payload_first_eight_bytes': data[payload:payload + 8].hex()}


def presentation_variant(data, presentation_hash):
    definitions = rows(data, 0x28, 0x80806489, 24)
    matches = [(index, at) for index, at in enumerate(definitions)
               if u32(data, at) == presentation_hash]
    if len(matches) != 1:
        raise ValueError('presentation identity is not unique')
    index, at = matches[0]
    result = []
    # Native layout descriptor at RVA3774700 gives sizeof(8080648B)==0x90.
    for variant in rows(data, at + 8, 0x8080648B, 0x90):
        exclude_count = struct.unpack_from('<Q', data, variant + 0x48)[0]
        if exclude_count != 0:
            raise ValueError('nonempty recipient exclusions require separate class qualification')
        result.append({'offset': variant,
                       'localized_hash': f'{u32(data, variant + 8):08X}',
                       'localized_container': f'{u32(data, variant + 0x18):08X}',
                       'argument_resource': f'{u32(data, variant + 0x20):08X}',
                       'local_player_filter': data[variant + 0x40],
                       'recipient_predicate': f'{u32(data, variant + 0x44):08X}',
                       'excluded_recipient_predicates': [],
                       'native_text_mode_58': data[variant + 0x58],
                       'proximity_subject': f'{u32(data, variant + 0x5C):08X}',
                       'proximity_radius': struct.unpack_from('<f', data, variant + 0x60)[0],
                       'channel': f'{u32(data, variant + 0x64):08X}',
                       'replacement_group': f'{u32(data, variant + 0x68):08X}',
                       'replacement_priority': u32(data, variant + 0x6C),
                       'enablement_hash': f'{u32(data, variant + 0x70):08X}'})
    return {'presentation_row': index, 'variants': result,
            'native_delivery_and_receipt_qualified': False}
