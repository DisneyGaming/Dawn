"""Publish one native NPC animation development command through server authority.

Use the current ev=activity_npc_animation ready/result line for boot, owner,
incarnation and animation revision. Population revision is a different service.
No native addresses, clip calls or process-memory writes are involved.
"""
import argparse
import os
from pathlib import Path


def command_text(boot, owner, incarnation, revision, request, registry, slot, action):
    if not (0 < boot < 2**64 and 0 < owner < 2**64 and 0 < incarnation < 2**64
            and 0 < revision < 2**64 and 0 < request < 2**64
            and 0 < registry < 0xFFFFFFFF and registry != 0x811C9DC5
            and 0 <= slot <= 32767 and 0 <= action < 2**32):
        raise ValueError("Invalid identity, revision, native controller or action ID")
    return f"npc1 {boot:016X} {owner:016X} {incarnation} {revision} {request} {registry:08X} {slot} {action}\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--boot", type=lambda x: int(x, 16), required=True)
    parser.add_argument("--owner", type=lambda x: int(x, 16), required=True)
    parser.add_argument("--incarnation", type=int, required=True)
    parser.add_argument("--revision", type=int, required=True)
    parser.add_argument("--request", type=int, required=True)
    parser.add_argument("--registry", type=lambda x: int(x, 16), default=0x564C6ECE)
    parser.add_argument("--slot", type=int, default=2)
    parser.add_argument("--action", type=int, choices=(0, 1), required=True,
                        help="1: authored Vance probe; 0: explicitly stop that controller")
    parser.add_argument("--root", type=Path, default=Path(r"D:\Destiny3\bin\x64\Dawn"))
    parser.add_argument("--print-only", action="store_true")
    args = parser.parse_args()
    text = command_text(args.boot, args.owner, args.incarnation, args.revision, args.request,
                        args.registry, args.slot, args.action)
    if not args.print_only:
        root = args.root.resolve(strict=True)
        staged, target = root / "activity-dev.txt.new", root / "activity-dev.txt"
        if staged.is_symlink() or target.is_symlink():
            raise ValueError("Command files must be regular local files")
        staged.write_text(text, encoding="ascii")
        os.replace(staged, target)
    print(text, end="")
    print("Printed only." if args.print_only else
          "Submitted only. Check server acceptance and native playback; publication is not readiness.")


if __name__ == "__main__":
    main()
