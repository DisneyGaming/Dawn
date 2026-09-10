"""Compare authored ambient source variants while investigating faction war.

Candidate discovery uses the ambient agent's inventory. Each source descriptor
and selector is independently checked in package bytes. Similar names are only
a comparison aid: they do not establish mutual exclusion, scheduling or waves.
"""
import argparse
import hashlib
import json
import re
from pathlib import Path
import sys

from mercury_faction_battle_catalog import source_choices, u32


def installed_components(reader):
    """Use the checked-in candidate catalog; verify identities and names in installed bytes."""
    root = Path(__file__).resolve().parents[2]
    source = root / 'Sunrise/src/state/activity/coo/mercury_ambient_catalog.h'
    text = source.read_text()
    registries = re.findall(
        r'\{"mercury_freeroam",0x80F4696A,0x([0-9A-F]+),0x([0-9A-F]+),0xA83A9175,15,kSlots_([0-9A-F]+)\}', text)
    sources = dict(re.findall(r'kSources_([0-9A-F]+)\{\{(.*?)\}\};', text, re.S))
    if not registries:
        raise ValueError('installed Mercury candidate catalog is empty')
    objects = {}
    for key, tag, slot_key in registries:
        if key != slot_key or key not in sources:
            raise ValueError('candidate registry/source catalog mismatch')
        raw, cls = reader.read_tag(int(tag, 16))
        if cls != 0x80809462 or u32(raw, 12) != int(key, 16):
            raise ValueError('installed ambient object identity mismatch')
        components = []
        for slot, resource in re.findall(r'\{(\d+),\d+,\d+,0x([0-9A-F]+)\}', sources[key]):
            data, cls = reader.read_tag(int(resource, 16))
            if cls != 0x80809C36:
                raise ValueError('installed source resource class mismatch')
            definition = source_choices(data)['definition_offset']
            if (u32(data, definition + 48) != int(key, 16) or data[definition + 52] != 1
                    or int.from_bytes(data[definition + 54:definition + 56], 'little') != int(slot)):
                raise ValueError('installed source slot identity mismatch')
            # Names are retained from the resource itself, never assembled from a hotspot label.
            names = re.findall(rb'pf_lighthouse_(?:ca|vx)_[A-Za-z0-9_.\[\]]+\x00', data)
            if len(names) != 1:
                raise ValueError('installed source name is missing or ambiguous')
            components.append(dict(slot_type=1, resource=resource,
                resource_name=names[0][:-1].decode('ascii'),
                resource_sha256=hashlib.sha256(data).hexdigest()))
        if not components:
            raise ValueError('candidate source list is empty')
        objects[tag] = dict(regkey=key, bubbles=[15], components=components,
                            object_sha256=hashlib.sha256(raw).hexdigest())
    return dict(objects=objects, discovery_catalog=str(source.relative_to(root)),
                discovery_catalog_sha256=hashlib.sha256(source.read_bytes()).hexdigest())


def extract(reader, inventory):
    sources = []
    for tag, obj in inventory['objects'].items():
        if obj['bubbles'] != [15]:
            continue
        for component in obj['components']:
            name = component.get('resource_name') or ''
            if component.get('slot_type') != 1 or not name.startswith(('pf_lighthouse_ca_', 'pf_lighthouse_vx_')):
                continue
            raw, cls = reader.read_tag(int(tag, 16))
            if cls != 0x80809462 or u32(raw, 12) != int(obj['regkey'], 16):
                raise ValueError('ambient object identity')
            data, cls = reader.read_tag(int(component['resource'], 16))
            if cls != 0x80809C36:
                raise ValueError('ambient source resource class')
            decoded = source_choices(data)
            at = decoded['definition_offset']
            if u32(data, at + 48) != int(obj['regkey'], 16) or data[at + 52] != 1:
                raise ValueError('ambient source scoped identity')
            sources.append({'object': tag, 'registry': obj['regkey'], 'name': name,
                            'resource': component['resource'],
                            'resource_sha256': hashlib.sha256(data).hexdigest(),
                            'hotspot_name': '_hotspot' in name, **decoded})
    named_pairs = []
    for source in sources:
        if not source['hotspot_name']:
            continue
        normal_name = source['name'].replace('_hotspot', '')
        normal = [s for s in sources if s['name'] == normal_name]
        if len(normal) != 1:
            raise ValueError('ambiguous name-only ambient comparison')
        named_pairs.append({'ordinary_resource': normal[0]['resource'], 'hotspot_resource': source['resource'],
                            'pairing_evidence': 'matching source names after removing _hotspot',
                            'authored_selection_relationship': None})
    return {'sources': sources, 'name_comparisons': named_pairs,
            'all_source_variants_identical': all(c['all_six_choices_identical'] for s in sources for c in s['categories']),
            'faction_battle_source_order': None, 'hotspot_activation_condition': None,
            'source_request_counts': None, 'activation_supported': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--reader-dir', type=Path, default=Path('D:/Sunrise-work/scripts'))
    parser.add_argument('--inventory', type=Path, help='Optional prior component inventory; default verifies the installed catalog candidates')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    sys.path.insert(0, str(args.reader_dir.resolve()))
    from pkg import Reader
    reader = Reader()
    inventory = json.loads(args.inventory.read_text()) if args.inventory else installed_components(reader)
    result = extract(reader, inventory)
    args.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(f'{len(result["sources"])} source variants compared; '
          f'all six alternatives identical={result["all_source_variants_identical"]}; '
          'no source ordering or activation asserted.')


if __name__ == '__main__':
    main()
