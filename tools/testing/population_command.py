"""Submit one explicit development request to the native server population service.

Use the owner, incarnation and revision printed by ev=activity_population in the
current run. This file interface does not access process memory or fake receipts.
"""
import argparse
import os
from pathlib import Path


def command_text(boot, owner, incarnation, revision, request, registry, slot, target):
    if not (0 < boot < 2**64 and 0 < owner < 2**64 and 0 < incarnation < 2**64
            and 0 < revision < 2**64 and 0 < request < 2**64
            and 0 < registry < 2**32 and 0 <= slot <= 32767 and 1 <= target <= 63):
        raise ValueError("Invalid identity, revision, source or cumulative target")
    return f"v2 {boot:016X} {owner:016X} {incarnation} {revision} {request} {registry:08X} {slot} {target}\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot", type=lambda x: int(x, 16), required=True)
    parser.add_argument("--owner", type=lambda x: int(x, 16), required=True)
    parser.add_argument("--incarnation", type=int, required=True)
    parser.add_argument("--revision", type=int, required=True)
    parser.add_argument("--request", type=int, required=True)
    parser.add_argument("--registry", type=lambda x: int(x, 16), required=True)
    parser.add_argument("--slot", type=int, required=True)
    parser.add_argument("--target", type=int, required=True)
    parser.add_argument("--root", type=Path, default=Path(r"D:\Destiny3\bin\x64\Sunrise"))
    args = parser.parse_args()
    text = command_text(args.boot, args.owner, args.incarnation, args.revision, args.request,
                        args.registry, args.slot, args.target)
    root = args.root.resolve(strict=True)
    staged = root / "activity-dev.txt.new"
    target = root / "activity-dev.txt"
    if staged.is_symlink() or target.is_symlink():
        raise ValueError("Command files must be regular local files")
    staged.write_text(text, encoding="ascii")
    os.replace(staged, target)
    print(text, end="")
    print("Submitted only. Check the server result and native observations before issuing another request.")


if __name__ == "__main__":
    main()
