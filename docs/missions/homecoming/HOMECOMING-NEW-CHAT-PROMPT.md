# New-Chat Prompt — paste this to start the server-side build

---

I'm reverse-engineering Destiny 2 (Season of Arrivals) to revive the **Homecoming** mission offline
under **Sunrise** (a `steam_api64` proxy DLL mod). This is authorized personal RE on my own machine.
A prior session mapped the whole problem end-to-end. **Read `C:\Destiny 2 Development\HOMECOMING-FINDINGS.md`
in full before doing anything — it has every confirmed finding, address, and dead-end. Do not
re-derive what it already establishes.**

## The goal for THIS chat

Build a **server-side authored activity-selection injection** in Sunrise's embedded BAP server, so
the game's activity manager for **identity 1** enters **authored "mode 1"** (not local mode 6)
naturally — no forcing. The route is driven by the **message-1 global activity-state selection push**
(NOT service 6 — see the findings, §6 dead-ends). Forcing the route byte / mode bit is a confirmed
dead-end: it prunes the session. The genuine authored descriptor must pass through before route
commit.

## The immediate blocker to solve first: the destination predicate `0xC068F0`

This is the live problem (findings §7). `FUN_7FF618C768F0(activityIndex, name)` validates whether the
**current destination datum matches a requested activity**, and it **rejects** the 282→266
(CHOSEN→Homecoming) transition because, when it runs, the live destination datum is not yet Homecoming
(266). The route only commits authored if this predicate accepts.

**First tasks (in order):**
1. Read `HOMECOMING-FINDINGS.md`, then re-read `predicate_out.txt` (already generated) — the predicate
   and its gate/comparison callees are decompiled there.
2. Build a **runtime probe** hooking `FUN_7FF618C768F0` (base `0x7FF618070000`): log `param_1`
   (activity index requested), the resolved destination datum `puVar5`, its name, the gate result,
   and the return value — during a Homecoming override load. This shows WHETHER it rejects at the gate
   (activity-client slot `manager+0x27D0+identity*0x2D0 == -1`) or at the name/index comparison, and
   what the live destination actually is vs 266.
3. From that, determine what must set the destination datum to Homecoming (266) before this predicate
   runs — i.e. what the message-1 authored selection push must contain.
4. Implement the server-side message-1 authored push in `server/bap/encrypted/push/activity/` so the
   client's native producer builds the authored 0x1B0 descriptor and the predicate accepts.

## Hard constraint to keep in mind

The authored 0x1B0 descriptor's "opaque authored data" (~248 bits, the 620-bit vs 372-bit delta)
CANNOT be sourced offline (not in content tags, not capturable). Every approach must either derive it
or route around needing it. This is the fundamental risk — surface it early, don't hand-wave it.

## Environment & workflow (details in findings §2–3)

- Repo `C:\Destiny 2 Development\Sunrise-src`, branch `spawner`. Build:
  `powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\sunrise-dev.ps1" -Config Release`
  (`-BuildOnly` to compile while the game runs; `-Restore` to roll back a bad DLL).
- Always `md5sum` the deployed vs built DLL to confirm deploy landed (findings §2).
- Log at `bin\x64\Sunrise\logs\sunrise.log`; probes emit `ev=…` lines.
- Dump: `destiny2_unpacked.bin`, base `0x7FF618070000`. Ghidra headless command + scripts dir in
  findings §3. Python RE helpers (`d2dis.py`, `callers.py`, `siggen.py`) in the scratchpad.
- Existing probes and their log stages are in findings §10 — reuse `mgrprobe` (manager census:
  `path=local(mode6)` vs `authored(mode1)`) and `hcprobe` to measure whether each change moves the
  needle.

## How I want you to work

- **Instrument first, then change.** Every prior win came from a probe that measured reality before
  editing. Build the predicate probe, read the log, THEN act.
- **One measurable module at a time.** Build → deploy → I run it → you read the log → iterate. I run
  the game; you can't.
- **Be honest about the ceiling.** Even George (the collaborator whose pipeline we're rebuilding) never
  got playable content — only "reaches the mission transition, world loads." Don't oversell.
- Save durable findings to memory as you go.

Start by reading `HOMECOMING-FINDINGS.md` and `predicate_out.txt`, then propose the predicate probe.
```
