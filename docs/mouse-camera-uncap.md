# Uncapping the mouse camera — the direct-angle fix (part 8, 2026-09-04)

The operator's mouse camera "felt slow even at MOUSE SENS 10, and raising it did
nothing." This records why, and the fix.

## Why input scaling could not help

The mouse feeds the game's right-stick source, and the camera update
`sub_82470DC0` turns that into a turn rate through a **radial stick model**:

```
82470FEC  fmuls  f0, f30, f30        ; yaw^2
82470FF0  fmadds f0, f29, f29, f0    ; + pitch^2
82470FFC  fsqrts f30, f0             ; magnitude = sqrt(yaw^2 + pitch^2)
...       fsel   f9, f8, f9, f28     ; clamp magnitude to f28 = 1.0  (0x8200747C)
```

The (yaw, pitch) input is treated as a 2D vector whose **magnitude is clamped to
1.0**, then run through a cubic response curve and a fixed max turn-rate. So:

* Scaling one axis at the source (the first attempt — a gain on the float command
  query `sub_827FFE90` for command IDs 216/217) just makes a longer vector that the
  clamp normalises straight back. **8x and 20x produced identical motion** — the
  proof the cap is downstream of the input. That approach was removed.
* Pegging the stick with the mouse already sits at the clamp. What felt "slow" was
  the game's own maximum gamepad turn rate; nothing upstream can exceed it.

Confirmed empirically: instrumenting the query hook showed it scaling raw values up
to ±32 to ±650 with no change in feel (the clamp ate it).

## The fix: add to the camera angle directly, past the clamp

`sub_82470DC0` keeps the persistent camera angles in the object passed in `r6`
(`r31` inside the callee):

```
8247113C  fadds f0, f8, f12          ; yaw  = prev(+0x40) + clamped yaw delta
82471140  stfs  f0, 0x40(r31)        ; -> r6+0x40  (YAW, radians)
82471150  fadds f11, f7, f11         ; pitch = prev(+0x44) + clamped pitch delta
82471154  stfs  f11, 0x44(r31)       ; -> r6+0x44  (PITCH, radians)
```

We strong-hook `sub_82470DC0`, capture `r6` on entry, and AFTER the game's own
(clamped) update add our uncapped mouse delta straight onto `r6+0x40` / `r6+0x44`.
Adding to the angle *integral* is stable: the game's smoother reads its own separate
state at `+0x48/+0x4c`, not these accumulators, so there is no feedback loop — the
instability that would have come from scaling the smoothed delta is avoided entirely.

Raw per-poll mouse deltas reach the hook via `NativeKbm_AddMouseLook` (fed from
`window.cpp`'s existing relative-mouse block, alongside the unchanged stick feed —
the stick feed stays so the engine's "camera is being moved" state still sets).

### Units, sign, scale (all measured, not guessed)

* **Radians.** The update applies a deg->rad constant internally, and a headless
  calibration read the yaw field moving 0 -> -0.45 during the attract camera — small
  values consistent with radians. `scale = 0.00027` rad per (mouse-count x sens)
  is the operator's dialed-in landing (SENS 5 ~= 0.135 rad / 100 counts).
* **Sign.** The field-delta sign is OPPOSITE the command-input sign the stick path
  used, so the naive `+=` came out inverted on both axes. Corrected default is
  `sx=-1, sy=+1` (mouse-right = look right, mouse-down = look down), confirmed in
  play. `CW_KBM_INVERT_X` / `CW_KBM_INVERT_Y` flip each live.
* **Gated** to `NativeKbm_Active() && MouseDeviceActive() && Settings_MouseCam()`
  and a sane `r6` range (the camera can live in the physical-alias heap — calibration
  saw it at `0xA6CCD418` — so the upper bound is generous). A controller's camera is
  never touched. The per-frame accumulator is drained once (exchange-to-0), so if the
  function runs for more than one camera in a frame only the first takes the delta.

### Controls

| env | effect |
|---|---|
| MOUSE SENS row (live) | linear multiplier on the look speed (1..10) |
| `CW_KBM_LOOK_SCALE=<f>` | overrides the 0.00027 base (coarse speed) |
| `CW_KBM_INVERT_X=1` | flip horizontal |
| `CW_KBM_INVERT_Y=1` | flip vertical |
| `CW_KBM_CAM_TRACE=1` | log applied yaw/pitch deltas |

## Not addressed

Pitch is not clamped by us; the game may or may not limit look-up/down at its matrix
build. No over-rotation was reported in play. If a pitch limit is ever wanted, clamp
`r6+0x44` to a range here — but confirm the game does not already do it first.
