"""Read captured 80807ECC source deltas; never infer kills from consumed requests."""
import argparse
import json
import re
from pathlib import Path

ENTRY = re.compile(r"t=(\d+).*sensor_sense_entry.*key=0x([0-9A-F]+) slot=1/(\d+) body_bits=(\d+) body=([^ ]+)")


def decode(width, words):
    chunks = []
    remaining = width
    for word in words:
        count = min(64, remaining)
        if not count:
            break
        if word >= 1 << count:
            raise ValueError("chunk exceeds declared width")
        chunks.append(f"{word:0{count}b}")
        remaining -= count
    if remaining:
        raise ValueError("log body truncated")
    bits = "".join(chunks)
    pos = 0

    def read(count):
        nonlocal pos
        if pos + count > len(bits):
            raise ValueError("incomplete field")
        value = int(bits[pos:pos + count], 2)
        pos += count
        return value

    result = {"delta": bool(read(1))}
    if result["delta"]:
        fields = {}
        for offset, count in zip(("00", "04", "08", "0C", "10", "14"), (31, 31, 31, 6, 7, 31)):
            if read(1):
                fields[offset] = read(count)
        result["fields"] = fields
        result["state18"] = read(2) - 1
        result["state19"] = read(3) - 1
        result["flags"] = [bool(read(1)) for _ in range(3)]
        if read(1):
            count = read(4)
            if count > 8:
                raise ValueError("consumed array overflow")
            result["consumed"] = [read(32) - 2**31 for _ in range(count)]
        if read(1):
            result["quantized44"] = {i: read(7) for i in range(24) if read(1)}
    result["revision"] = read(32)
    if pos != len(bits):
        raise ValueError("unexpected trailing fields")
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path)
    parser.add_argument("--registry", action="append", default=[])
    args = parser.parse_args()
    allowed = {value.upper().removeprefix("0X") for value in args.registry}
    seen = set()
    for line in args.log.open(encoding="utf-8", errors="replace"):
        match = ENTRY.search(line)
        if not match:
            continue
        tick, key, slot, width, raw = match.groups()
        if allowed and key not in allowed:
            continue
        identity = (tick, key, slot, width, raw)
        if identity in seen:
            continue
        seen.add(identity)
        row = {"tick": int(tick), "registry": key, "slot": int(slot)}
        try:
            row.update(decode(int(width), [int(word, 16) for word in raw.split(",")]))
        except ValueError as error:
            row["error"] = str(error)
        print(json.dumps(row))


if __name__ == "__main__":
    main()
