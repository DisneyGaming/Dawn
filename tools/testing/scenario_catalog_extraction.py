"""Replay production roster extraction with installed package bytes, without a game.

The child runs the real C++ walker. Each requested tag is supplied by the existing
package reader; no cached or authored group is substituted for extraction.
Only fixtures and a report under the selected output directory are written.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/coo'))
import package_read

SIZES = [140, 12, 76, 68, 294, 8, 8, 2, 8, 16, 220, 928, 4,
         1366, 30730, 44, 72, 20, 56, 12, 64, 48, 28]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    cache = (ROOT / 'Dawn/cache/build_data.bin').read_bytes()
    version = struct.unpack_from('<I', cache, 8)[0]
    # These versions differ in extraction identity, not record layout.
    if cache[:8] != b'DAWNDATA' or version not in (54, 55, 56):
        raise ValueError('Review the cache layout before reading a new version')
    counts = struct.unpack_from('<23I', cache, 92)
    if 201 + sum(c * s for c, s in zip(counts, SIZES)) != len(cache):
        raise ValueError('Cache record sizes do not match')
    offset = 201 + sum(c * s for c, s in zip(counts[:13], SIZES[:13]))
    fixture = args.output / 'scenarios.bin'
    fixture.write_bytes(struct.pack('<I', counts[13]) + cache[offset:offset + counts[13]*SIZES[13]])
    read_cache = {}
    failures = []
    output = []
    requests = 0
    started = time.monotonic()
    extracted = args.output / 'extracted.bin'
    with (args.output / 'stderr.log').open('wb') as errors:
        child = subprocess.Popen([str(args.binary), str(fixture), str(extracted)], cwd=ROOT,
                                 stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=errors)
        try:
            for raw in child.stdout:
                line = raw.decode('ascii').strip()
                match = re.fullmatch(r'READ ([0-9A-F]{8})', line)
                if not match:
                    output.append(line)
                    print(line, flush=True)
                    continue
                requests += 1
                tag = int(match[1], 16)
                if tag not in read_cache:
                    try:
                        cls, blob = package_read.read(tag)
                        read_cache[tag] = struct.pack('<II', cls, len(blob)) + blob
                    except (ValueError, AssertionError, FileNotFoundError, struct.error) as error:
                        failures.append({'tag': f'{tag:08X}', 'reason': str(error)})
                        read_cache[tag] = struct.pack('<II', 0, 0xFFFFFFFF)
                child.stdin.write(read_cache[tag])
                child.stdin.flush()
            code = child.wait(timeout=10)
        finally:
            if child.poll() is None:
                child.kill()
                child.wait()
    report = {'exit_code': code, 'cache_sha256': hashlib.sha256(cache).hexdigest(),
              'cache_version': version, 'scenarios': counts[13], 'read_requests': requests,
              'unique_tags': len(read_cache), 'unreadable_tags': failures,
              'elapsed_seconds': round(time.monotonic()-started, 3), 'output': output}
    if code == 0:
        produced = extracted.read_bytes()
        scenario_count, group_count = struct.unpack_from('<II', produced)
        if scenario_count != counts[13] or len(produced) != 8 + scenario_count*1366 + group_count*30730:
            raise ValueError('Production extraction export has an invalid record extent')
        old_end = offset + counts[13]*1366 + counts[14]*30730
        old_group_start = offset + counts[13]*1366
        new_group_start = 8 + scenario_count*1366
        def group_id(row):
            # Equivalent descriptor layouts may be discovered first through a
            # different object alias. The production deduplicator ignores that tag.
            return hashlib.sha256(row[:4] + row[8:]).digest()
        old_groups = [cache[old_group_start+i*30730:old_group_start+(i+1)*30730]
                      for i in range(counts[14])]
        new_groups = [produced[new_group_start+i*30730:new_group_start+(i+1)*30730]
                      for i in range(group_count)]
        identities = {group_id(row) for row in new_groups}
        lost = [f'{struct.unpack_from("<I", row)[0]:08X}' for row in old_groups
                if group_id(row) not in identities]
        if lost:
            raise ValueError(f'Previously extracted slot layouts disappeared: {lost}')
        def core_keys(row, groups):
            return ([groups[struct.unpack_from('<H', row, 468+i*2)[0]][:4]
                     for i in range(row[47])],
                    [groups[struct.unpack_from('<H', row, 476+i*2)[0]][:4]
                     for i in range(row[49])])
        changed = []
        for i in range(scenario_count):
            before = cache[offset+i*1366:offset+(i+1)*1366]
            after = produced[8+i*1366:8+(i+1)*1366]
            if core_keys(before, old_groups) != core_keys(after, new_groups):
                changed.append(before[:40].split(b'\0')[0].decode('ascii'))
        if changed:
            raise ValueError(f'Core roster membership changed: {changed}')
        report['preserved_existing_slot_layouts'] = len(old_groups)
        report['preserved_core_rosters'] = scenario_count
        isolated = bytearray(cache[:offset] + produced[8:] + cache[old_end:])
        current_version = int(re.search(r'kCacheFormatVersion\s*=\s*(\d+)',
            (ROOT/'Dawn/src/state/build_data/cache/records/version.h').read_text())[1])
        struct.pack_into('<I', isolated, 8, current_version)
        struct.pack_into('<I', isolated, 92+14*4, group_count)
        # It is not a deployable cache. Deliberately remove both producer identities.
        isolated[28:92] = bytes(64)
        checksum = 14695981039346656037
        for byte in isolated[184:193] + isolated[201:]:
            checksum = ((checksum ^ byte) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
        struct.pack_into('<Q', isolated, 193, checksum)
        cache_fixture = args.output/'scenario-cache-test-only.bin'
        cache_fixture.write_bytes(isolated)
        report['test_cache'] = str(cache_fixture)
        report['test_cache_sha256'] = hashlib.sha256(isolated).hexdigest()
        report['test_cache_provenance'] = 'Production scenario extraction replay; other domains from installed cache; producer identities deliberately cleared; never install'
    (args.output / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f"Replay finished: {requests} reads, {len(read_cache)} tags, {report['elapsed_seconds']}s", flush=True)
    raise SystemExit(code)


if __name__ == '__main__':
    main()
