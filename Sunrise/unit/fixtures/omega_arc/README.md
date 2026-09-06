# Held Arc charge capture

Captured read-only from PID 48516 on 2026-09-05, installed build 4AECEE48,
immediately after the user confirmed that they were holding the Arc charge.
Full evidence: build/omega-full-20260905/arc-live-48516-1788655743.

- held-item-48516.bin: first 0x4A0 bytes of native 80F66667 / 80804221 / +598.
  Full item entity 46FAA21A, component 18F9E57B, state 1 at +470.
- held-inventory-48516.bin: 0x30-byte header of holder 7FF9E41B,
  80F56156 / 80803E64 / +468, player entity 1DFAA259.
- The item's world attachment parent +3C was also 1DFAA259. Both item and
  player world records retained their complete entity handles at +C.

Tests remap handles to their isolated native-reader fixture. They preserve
the captured state and typed inventory context, check ownership agreement,
reject stale identities, and exercise the native consumed-request observer.
This proves recognition of the captured held state, not live dunk acceptance.
