"""Capture native reflection evidence and independently construct type-42 vectors.

Read-only package/executable inputs. No game process access. The local RE tools
are not distributed; pass their directory explicitly. Outputs are local evidence.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--re-scripts", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    sys.path.insert(0, str(args.re_scripts))
    import schema
    from pkg import Reader

    handles = [0x80809586, 0x80809589, 0x80809588, 0x80809587, 0x8080958A]
    schemas = {}
    for handle in handles:
        record = schema.find_blob(handle)
        assert record is not None
        fields = schema.parse_fields(record)
        schemas[handle] = (schema.codec_info(record), fields)
    root = schemas[0x80809586][1]
    assert [(f["type_code"], f["has_presence_bit"], f["sub_handle"]) for f in root] == [
        (1, 1, 0x80809589), (1, 1, 0x80809588)]
    seq = schemas[0x80809589][1]
    assert [(f["type_code"], f["param_18"], f["param_1c"]) for f in seq] == [
        (9, 0, 32), (9, 0, 32), (5, -0x80000000, 32)]
    events = schemas[0x80809588][1]
    assert [(f["type_code"], f["param_18"], f["param_1c"]) for f in events] == [
        (5, 0, 3), (1, 1, 0)]
    assert schemas[0x80809587][0]["array_len"] == 4
    assert schemas[0x8080958A][0]["array_len"] == 2

    # Walk reflected nested/primitive descriptors. This adapter exercises only
    # the supported zero-count event form; refusing nonempty lists is deliberate.
    def encode(handle, values):
        result = ""
        info, fields = schemas[handle]
        assert not info["array_len"]
        assert len(fields) == len(values)
        for field, value in zip(fields, values):
            if field["has_presence_bit"]:
                result += "0" if value is None else "1"
                if value is None:
                    continue
            kind = field["type_code"]
            if kind == 1:
                if field["param_18"]:
                    assert value == []
                else:
                    result += encode(field["sub_handle"], value)
            elif kind in (5, 9):
                width = field["param_1c"]
                biased = value - field["param_18"]
                assert 0 <= biased < (1 << width)
                result += format(biased, f"0{width}b")
            else:
                raise AssertionError(f"Unsupported reflection kind {kind}")
        return result

    reader = Reader()
    bank, bank_class = reader.read_tag(0x80EC120B)
    definition, definition_class = reader.read_tag(0x80EC11F7)
    assert bank_class == 0x8080815F and len(bank) == 164
    assert struct.unpack_from("<I", bank, 0x6C)[0] == 0xAFB11A12
    assert struct.unpack_from("<I", bank, 0xA0)[0] == 0x010B0F07
    assert definition_class == 0x80809C36
    assert struct.unpack_from("<I", definition, 0x224)[0] == 0x010B0F07
    evidence = {
        "schema": {f"{key:08X}": {"info": info, "fields": [
            {k: (v.hex() if isinstance(v, bytes) else v) for k, v in field.items()}
            for field in fields]} for key, (info, fields) in schemas.items()},
        "bank_sha256": hashlib.sha256(bank).hexdigest(),
        "definition_sha256": hashlib.sha256(definition).hexdigest(),
        "playback_verified": False,
    }
    vectors = []
    cases = [(0x811C9DC5, 0x811C9DC5, 0),
             (0x010B0F07, 0x811C9DC5, 1),
             (0x010B0F07, 0x811C9DC5, 2),
             (0x010B0F07, 0x811C9DC5, 0x7FFFFFFF),
             (0, 0, 1), (0xFFFFFFFF, 0xFFFFFFFF, 17),
             (0x811C9DC5, 0x811C9DC5, 27)]
    for first, second, counter in cases:
        bits = encode(0x80809586, [[first, second, counter], [0, []]])
        padded = bits + "0" * ((8 - len(bits) % 8) % 8)
        packet = int(padded, 2).to_bytes(len(padded) // 8, "big")
        vectors.append(f"{first:08X} {second:08X} {counter} {len(bits)} {packet.hex()}")
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "native_npc_animation_schema.json").write_text(json.dumps(evidence, indent=2))
    (args.output / "native_npc_animation_vectors.txt").write_text("\n".join(vectors) + "\n")
    print(f"Verified five native schemas and Vance's bank; wrote {len(vectors)} independent vectors.")


if __name__ == "__main__":
    main()
