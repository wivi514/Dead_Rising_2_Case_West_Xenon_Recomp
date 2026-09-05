# Laser gun / light saber — repeating "equip" sound (diagnosis, 2026-09-05)

**Status: FIXED 2026-09-05 (commit follows).** Root cause confirmed by measurement:
a hardware-loop XMA context (loopCount=254 measured on the laser cue) was invalidated
on buffer-consume like a one-shot, so the guest saw the voice stop and re-created the
context ~2/s. Xenia sustains such loops (operator confirmed it plays clean there —
the oracle). Fix: in `XmaDecodeOnePacket`, when `loopCount != 0`, rewind to the buffer
start and KEEP the buffer valid (counting the loop down; 0xFF = infinite), instead of
clearing the valid bit. One-shots (loopCount==0, all normal SFX/cinematic streaming)
are byte-for-byte unchanged. Control arm: `CW_XMA_NO_LOOP=1`. Operator-verified: the
laser gun / light saber now plays a continuous hum, no repeat.

## Symptom (operator)

With the laser gun (or light saber) equipped, the "taking out gun" cue plays
**repeatedly**, ~2 times a second, for as long as the weapon is held.

## What was measured (live, `~/DR2CW-troubleshooting/laser_audio.log`)

- `XMACreateContext` calls **bunch during the bug**: 120 total before, 244 after,
  **77 of them in the ~40 s the gun was out** (~2/s). Each cue re-trigger allocates a
  fresh XMA context, so the guest is genuinely re-playing the equip cue ~2/s — a
  re-trigger loop, **not** per-frame spam and **not** an XMA decode failure (audio
  decodes fine; it just restarts).
- **Input ruled out.** Hands-off (no keyboard/mouse) the sound keeps repeating, so it
  is NOT a stuck KB/M binding — the native KB/M work (parts 8) is not the cause. It is
  guest weapon/audio state, independent of input.
- The rate (~2/s ≈ the short cue's length) is consistent with the cue being
  **restarted each time it finishes**.

## Leading hypothesis

XMA **loop / end-of-stream lifecycle**. `runtime/kernel/audio.cpp` parses the loop
fields (DWORD0 `loop_count:8`, DWORD1 `loop_subframe_start/end/skip`) and already
documents a subtle finish-vs-loop trap around lines 766-790 ("...reads downstream as
'the voice finished' and destroys the evidence"; "a click and not a loop"). If the
laser weapon's cue is a LOOP the guest sets up once, and our decoder/context signals
"done" at the end of the first pass, the guest re-creates the context to restart it —
which presents exactly as a ~2/s repeating re-trigger.

## Next session plan

1. **Ask the oracle first** (gotcha 320 corollary): does Xenia play the laser gun's
   audio cleanly, or does it repeat there too? The operator has driven Xenia. One
   answer may retire or reframe this. (Laser gun is Case West content; Case Zero the
   demo may not have the weapon, so the sibling is a weak oracle here.)
2. Identify the specific cue: log the guest sound-play call for the equip/idle cue and
   the XMA context's loop fields at `XMACreateContext` for the laser weapon — is
   `loop_count` non-zero (a loop the guest expects us to sustain)?
3. If it is a loop: fix the context lifecycle so a looping voice is not reported
   finished at the first end-of-stream (compare against audio.cpp's existing
   fill/finish logic; the fix is in the same neighbourhood).
4. If `loop_count` is zero: the guest itself re-issues the cue — trace what weapon
   state gates it (animation-complete flag, a timer) and why it stays true.

## Note

Log captured to `~/DR2CW-troubleshooting/laser_audio.log` (delete after — it is a
diagnostic file). The shader-completion run was NOT interrupted for this; the audio
glitch does not block capture.
