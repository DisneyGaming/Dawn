"""Native integration inputs. Package fixtures are actual installed bytes, never captures."""
from pathlib import Path
import hashlib
import json
import re
import shutil

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / 'build/coo/native-r13-installed-fixtures'
POLICY = ROOT / 'Sunrise/scripts/mercury_freeroam.json'
CACHE = ROOT / 'Sunrise/cache/build_data.bin'

# These tests compare against outputs from the original executable or recorded live traffic.
# Compiling them is useful, but an absent evidence set must never be reported as a pass.
CAPTURE_ONLY = {
    'adventure_arrival_tests': 'original 90-byte live Adventure request (also needs current scenario cache)',
    'adventure_cancel_tests': 'original active/clear cue bodies and native manager entries',
    'adventure_cue_identity_tests': 'original helper component/input/row/self and manager snapshots',
    'adventure_cue_tests': 'original cue bodies, native decoder masks, manager entries and live request',
    'adventure_dialogue_tests': 'original dialogue decoded buffers/masks, cue entries and live request',
    'adventure_navigation_tests': 'original navigation wire bodies, decoded buffers and applied entries',
    'adventure_source_selection_tests': 'original start and abandon request captures',
    'forest_generator_progress_tests': 'original generator decoded buffers and six archived r12 sense packets',
    'native_npc_animation_tests': 'seven independently produced native NPC animation vectors',
    'public_event_cue_pending_tests': 'original delayed authority and pending/ready manager entries',
    'public_event_cue_progress_tests': 'original native cue manager entry',
    'public_event_cue_tests': 'original Adventure/Crossroads cue entries and decoded authority',
    'public_event_deferred_placement_tests': 'original before/after deferred placement snapshots',
    'public_event_engagement_tests': 'original engagement bodies, decoded states and before/after snapshots',
}
OPTIONAL_EVIDENCE = {
    'native_capture_feedback_tests': 'optional original capture and definition snapshots',
    'public_event_opening_progress_tests': 'optional original native cue manager entry',
    'public_event_incoming_tests': 'optional participant and cue manager native snapshots',
    'public_event_tests': 'optional original native apply snapshots; installed descriptors are supplied',
}
OUTPUT_DIRECTORY = {
    'forest_generator_exports', 'public_event_dialogue_tests', 'public_event_incoming_tests',
    'public_event_interaction_tests', 'public_event_music_tests', 'public_event_participant_tests',
    'public_event_sequence_tests', 'public_event_world_tests', 'world_device_service_tests',
}
PACKAGE_TESTS = {
    'adventure_tests', 'haunted_forest_launch_tests', 'haunted_forest_lifetime_tests',
    'haunted_forest_mode_tests', 'mission_launch_tests', 'mission_launch_arguments_tests',
    'public_event_tests',
}

class MissingEvidence(RuntimeError):
    """The test cannot run all mandatory checks with available inputs."""


def prepare():
    """Export verified installed tags once, retaining classes and hashes in a receipt."""
    marker = FIXTURES / 'manifest.json'
    if marker.exists():
        manifest = json.loads(marker.read_text())
        if all((FIXTURES / item['path']).is_file() and
               hashlib.sha256((FIXTURES / item['path']).read_bytes()).hexdigest() == item['sha256']
               for item in manifest):
            return FIXTURES
    import package_read
    FIXTURES.mkdir(parents=True, exist_ok=True)
    tags = {0x81327D63, 0x81327CF0, 0x80F5B993, 0x80C0127D,
            0x80F5B960, 0x80F5B956, 0x80F5B959, 0x80F5B95D,
            0x80F5B963, 0x80F5B966, 0x80F5B969, 0x80F5B96C, 0x80F5B96F,
            0x80F5B972, 0x80F5B975, 0x80F5B978, 0x80F5B97B, 0x80F5B97E,
            0x80F5B981, 0x80F5B984, 0x81550015, 0x8150A0A8, 0x8150A9FC}
    descriptor_sets = [
        ('Sunrise/src/server/runtime/activity/haunted_forest_registries.h', 'haunted'),
        ('Sunrise/src/state/activity/coo/mercury_public_event_registries.h', 'public-event'),
    ]
    descriptors = {}
    for source, folder in descriptor_sets:
        text = (ROOT / source).read_text()
        descriptors[folder] = {int(value, 16) for value in re.findall(
            r'\{\s*\d+\s*,\s*\d+\s*,\s*0x[0-9A-Fa-f]+\s*,\s*0x[0-9A-Fa-f]+\s*,\s*0x[0-9A-Fa-f]+\s*,\s*0x([0-9A-Fa-f]+)', text)}
        if not descriptors[folder]:
            raise RuntimeError(f'No descriptor tags extracted from {source}')
        tags.update(descriptors[folder])
    manifest = []
    for tag in sorted(tags):
        cls, data = package_read.read(tag)
        name = f'{tag:08X}.bin'
        (FIXTURES / name).write_bytes(data)
        manifest.append(dict(tag=f'{tag:08X}', cls=f'{cls:08X}', path=name,
                             size=len(data), sha256=hashlib.sha256(data).hexdigest()))
        for folder, members in descriptors.items():
            if tag not in members:
                continue
            directory = FIXTURES / folder
            directory.mkdir(exist_ok=True)
            payload = cls.to_bytes(4, 'little') + data if folder == 'public-event' else data
            (directory / name).write_bytes(payload)
            manifest.append(dict(tag=f'{tag:08X}', cls=f'{cls:08X}', path=f'{folder}/{name}',
                                 size=len(payload), sha256=hashlib.sha256(payload).hexdigest()))
    shutil.copyfile(ROOT / 'Sunrise/unit/fixtures/adventure_mercury_flags.json',
                    FIXTURES / 'adventure_mercury_flags.json')
    marker.write_text(json.dumps(manifest, indent=2))
    return FIXTURES


def prepare_metadata():
    """Follow production localization/artwork references and export unmodified tags."""
    import struct
    import package_read
    folder = FIXTURES / 'metadata'
    marker = folder / 'manifest.json'
    if marker.exists() and all((folder / name).is_file() for name in ('8132CC78.bin','8132D32F.bin','8132D828.bin')):
        rows = json.loads(marker.read_text())
        if all((folder / r['path']).is_file() and
               hashlib.sha256((folder / r['path']).read_bytes()).hexdigest() == r['sha256']
               for r in rows):
            return folder
    folder.mkdir(parents=True, exist_ok=True)
    exported = {}
    u32 = lambda b, o: struct.unpack_from('<I', b, o)[0]
    u64 = lambda b, o: struct.unpack_from('<Q', b, o)[0]
    rel = lambda b, o: o + struct.unpack_from('<q', b, o)[0]
    def array(b, o):
        count = u64(b, o)
        header = rel(b, o + 8)
        assert count <= 300000 and u64(b, header) == count
        return count, header + 16
    def read(tag):
        if tag in exported:
            return (folder / f'{tag:08X}.bin').read_bytes()
        cls, data = package_read.read(tag)
        name = f'{tag:08X}.bin'
        (folder / name).write_bytes(data)
        exported[tag] = dict(tag=f'{tag:08X}', cls=f'{cls:08X}', path=name,
                             size=len(data), sha256=hashlib.sha256(data).hexdigest())
        return data
    for tag in (0x8132CC78,0x8132D32F,0x8132D828):
        read(tag)
    globals_data = read(0x81A2926E)
    (folder / 'globals.bin').write_bytes(globals_data)
    read(0x81327CF0)
    child = lambda n: read(u32(globals_data, 16 + n * 16))
    display, types, banks, seasons = child(3), child(6), child(72), child(54)
    wanted_banks = {2417, 2095}
    count, start = array(display, 8)
    for i in range(count):
        record = rel(display, start + i * 16 + 8)
        detail = rel(display, record)
        wanted_banks.update(u32(display, detail + offset) for offset in (4, 12, 20))
    count, start = array(types, 8)
    wanted_banks.update(u32(types, start + i * 128 + 4) for i in range(count))
    count, start = array(seasons, 8)
    wanted_banks.update(u32(seasons, start + i * 72 + 36) for i in range(4, min(11, count)))
    for tag, offset in ((0x8161353B,132),(0x816135B4,132),(0x816134AB,132),(0x81613CF8,37676)):
        wanted_banks.add(u32(read(tag), offset))
    count, start = array(banks, 8)
    for bank in sorted(wanted_banks):
        if bank >= count:
            continue
        container = read(u32(banks, start + bank * 8 + 4))
        read(u32(container,24))
    artwork = (ROOT / 'Sunrise/src/client/content/activity/activity_presentation_build.h').read_text()
    style_text = artwork.split('styles{',1)[1].split('};',1)[0]
    styles = [int(x,16) for x in re.findall(r'0x([0-9A-Fa-f]+)',style_text)]
    def texture(tag):
        descriptor = read(tag)
        assert len(descriptor) == 40 and u32(descriptor,4) == 28
        read(int(exported[tag]['cls'],16))
    for style in styles:
        data = read(style)
        container = read(u32(data,16))
        read(u32(container,24))
        variants = read(u32(data,12))
        count, start = array(variants,8)
        assert count >= 3
        image = read(u32(variants,start+2*112+4))
        texture(u32(image,128))
    icon_map = read(0x81A291C2)
    count, start = array(icon_map,8)
    icons = [(12109,32,3),(13329,32,3),(13610,32,3),(14737,32,3),
             (10586,20,0),(5973,20,0),(5964,20,0),(5980,20,0),(11203,20,0),
             (16,20,0),(10541,32,3),(11305,32,3),(11578,32,3)]
    for index, field, variant in icons:
        source = read(u32(icon_map,start+index*24+16))
        image = read(u32(source,field))
        count, frames = array(image,32)
        count, textures = array(image,frames)
        texture(u32(image,textures+variant*4))
    (folder / 'classes.txt').write_text(''.join(f"{r['tag']} {r['cls']}\n" for r in exported.values()))
    rows = list(exported.values())
    for name in ('globals.bin','classes.txt'):
        data = (folder / name).read_bytes()
        rows.append(dict(path=name,size=len(data),sha256=hashlib.sha256(data).hexdigest()))
    marker.write_text(json.dumps(rows,indent=2))
    return folder


def prepare_scenario_cache():
    """Supplement a copy of the installed cache with two package-verified Mercury groups."""
    import struct
    import subprocess
    import package_read
    if not CACHE.is_file():
        raise MissingEvidence(f'Current scenario cache is unavailable: {CACHE}')
    output = FIXTURES / 'scenario-cache.bin'
    receipt = FIXTURES / 'scenario-cache.json'
    source_hash = hashlib.sha256(CACHE.read_bytes()).hexdigest()
    if receipt.exists() and output.exists():
        prior = json.loads(receipt.read_text())
        if prior['sourceSha256'] == source_hash and prior['sha256'] == hashlib.sha256(output.read_bytes()).hexdigest():
            return output
    # The compiler supplies field offsets and record widths from the current C++ types.
    directory = FIXTURES / 'cache-layout'
    directory.mkdir(parents=True, exist_ok=True)
    import verify
    command = [str(verify.MSBUILD), str(ROOT / 'tools/coo/cache_layout.vcxproj'),
               '/nologo', '/v:quiet', '/p:Configuration=Release', '/p:Platform=x64',
               f'/p:OutDir={directory}\\', f'/p:IntDir={directory / "obj"}\\']
    result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
    (directory / 'build.log').write_text(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError('Cache layout build failed: ' + result.stdout + result.stderr)
    layout = json.loads(subprocess.check_output([str(directory / 'cache_layout.exe')], text=True))
    data = bytearray(CACHE.read_bytes())
    unpack = lambda b, off: struct.unpack_from('<I', b, off)[0]
    records = layout['records']; header = records['Header']; fields = header['fields']
    if unpack(data, fields['version'][0]) != layout['version']:
        raise MissingEvidence('Installed cache format differs from current record layout')
    offset = header['size']
    for domain in layout['domains']:
        if domain['record'] == 'RosterGroupRecord':
            break
        offset += domain['size'] * unpack(data, fields[domain['count']][0])
    record = records['RosterGroupRecord']; rf = record['fields']; size = record['size']
    count = unpack(data, fields['rosterGroupCount'][0]); end = offset + count * size
    present = {unpack(data, offset + i * size) for i in range(count)}
    supplemental = []
    inputs = []
    definitions = [
        (0x2749BAAE, 0x80F5B993, 'Sunrise/src/state/activity/coo/adventure_mercury.h', 'kSlots'),
        (0xF25B938B, 0x80F5E5CD, 'Sunrise/src/state/activity/coo/mercury_registries.h', 'kTeleporters'),
    ]
    for key, object_tag, catalog_path, array_name in definitions:
        if key in present:
            continue
        cls, obj = package_read.read(object_tag)
        assert cls == 0x80809462 and unpack(obj,12) == key
        text = (ROOT / catalog_path).read_text()
        body = re.search(array_name + r'\{\{(.*?)\}\};', text, re.S).group(1)
        slots = re.findall(r'\{(\d+),(\d+),0x([0-9A-F]+),0x([0-9A-F]+),0x([0-9A-F]+),0x([0-9A-F]+)\}',body)
        assert slots
        row = bytearray(size)
        struct.pack_into('<IIH',row,0,key,object_tag,len(slots))
        for index, values in enumerate(slots):
            slot, kind = map(int, values[:2]); component, sense, auth, tag = [int(x,16) for x in values[2:]]
            cls, blob = package_read.read(tag)
            assert cls == 0x80809C36
            offsets = [o for o in range(0,len(blob)-127,4) if unpack(blob,o)==tag
                       and unpack(blob,o+8)==0x70 and unpack(blob,o+48)==key]
            assert len(offsets)==1
            at=offsets[0]
            assert (unpack(blob,at+4),unpack(blob,at+68),unpack(blob,at+72))==(component,sense,auth)
            assert struct.unpack_from('<HH',blob,at+52)==(kind,slot)
            # Both present schemas produce the native sense/auth flags used by these groups.
            assert sense != 0xFFFFFFFF and auth != 0xFFFFFFFF
            for field, value, width in [('slotTypes',kind,1),('slotFlags',3,1),('slotIndices',slot,2),
                ('descriptorTags',tag,4),('descriptorOffsets',at,4),('componentClasses',component,4),
                ('senseSchemas',sense,4),('authSchemas',auth,4)]:
                pos=rf[field][0]+index*width;row[pos:pos+width]=value.to_bytes(width,'little')
            inputs.append(dict(tag=f'{tag:08X}',sha256=hashlib.sha256(blob).hexdigest()))
        supplemental.append(row)
    data[end:end] = b''.join(supplemental)
    struct.pack_into('<I',data,fields['rosterGroupCount'][0],count+len(supplemental))
    # This is an isolated validation fixture with a complete payload checksum; live cache is untouched.
    checksum=14695981039346656037
    start,length=fields['constants']
    for byte in data[start:start+length]+data[header['size']:]:
        checksum=((checksum^byte)*1099511628211)&0xFFFFFFFFFFFFFFFF
    struct.pack_into('<Q',data,fields['payloadChecksum'][0],checksum)
    output.write_bytes(data)
    receipt.write_text(json.dumps(dict(source=str(CACHE),sourceSha256=source_hash,
        sha256=hashlib.sha256(data).hexdigest(),addedGroups=len(supplemental),
        provenance='test-only cache supplement from verified installed package descriptors',inputs=inputs),indent=2))
    return output


def arguments(name, output_directory):
    """Return executable arguments; raise explicitly for unavailable mandatory evidence."""
    if name in CAPTURE_ONLY:
        raise MissingEvidence(f'{name}: missing {CAPTURE_ONLY[name]}')
    output = Path(output_directory).resolve()
    output.mkdir(parents=True, exist_ok=True)
    if name in OUTPUT_DIRECTORY:
        return [str(output)]
    if name == 'public_event_initial_lifetime_tests':
        return [str(POLICY)]
    if name == 'public_event_initial_tests':
        return [str(POLICY), str(output)]
    if name in {'adventure_gateway_tests', 'adventure_overlay_tests'}:
        if not CACHE.is_file():
            raise MissingEvidence(f'{name}: current native scenario cache is unavailable: {CACHE}')
        return [str(prepare_scenario_cache()), str(output)]
    if name in PACKAGE_TESTS:
        folder = prepare()
        if name == 'mission_launch_tests':
            return [str(folder / '81327CF0.bin')]
        if name == 'mission_launch_arguments_tests':
            return [str(folder / '81327CF0.bin'), str(folder / '81550015.bin')]
        if name == 'haunted_forest_lifetime_tests':
            return [str(folder), str(output)]
        if name == 'haunted_forest_mode_tests':
            return [str(folder / 'haunted')]
        if name == 'public_event_tests':
            return [str(folder / 'public-event')]
        return [str(folder)]
    if name == 'mission_launch_metadata_tests':
        return [str(prepare_metadata())]
    if name in {'mission_launch_visual_tests', 'mission_launch_ui_tests'}:
        font = ROOT / 'fonts/NeueHaasUnicaW1G-Regular.otf'
        if not font.is_file():
            raise MissingEvidence(f'{name}: installed game font is unavailable: {font}')
        if name == 'mission_launch_ui_tests':
            return [str(font)]
        return [str(prepare_metadata()), str(font), str(output), str(prepare() / '81550015.bin')]
    return []


if __name__ == '__main__':
    print(prepare())
    print(json.dumps({'captureOnly': CAPTURE_ONLY, 'optionalEvidence': OPTIONAL_EVIDENCE}, indent=2))
