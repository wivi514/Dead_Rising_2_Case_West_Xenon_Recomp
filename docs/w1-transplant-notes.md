# W1 — the runtime transplant

**2026-08-15, session 2.** Case Zero's `runtime/` lifted into this port, retargeted, and
taken through its link gate and a first boot. The plan item is `docs/port-plan.md` W1.

**Result: the gate passes and the guest runs.** `cw_runtime --smoke` resolves every
generated symbol, and a real boot runs 60 s without crashing, loads 87 `.big` archives
through our VFS, and delivers vblank interrupts to the guest's own registered callback.
It does not present a frame yet — see §6.

---

## 1. What came across, and what did not

`rsync` of `runtime/` minus `build/`: **52 files, ~34,500 lines**. Everything in the
portable core transplanted without modification: the kernel HLE, the flat memory map and
o1heap arenas, guest-thread bootstrap, VFS, content/save layer, XMA audio, the PM4
command processor, the Vulkan renderer, and the SDL window/present/input seam.

**Four modules did not, and they are parked in `runtime/port-pending/`** with a README:

| file | lines of per-title RE |
|---|---|
| `cpu/guest_probe.cpp` | 48 hooked guest addresses |
| `cpu/debug_tunables.cpp` | 29 — the debug-menu / DebugJump / AutoChuck toolchain |
| `gpu/d3d_hooks.cpp` | the D3D arm's hook table (phase A) |
| `gpu/d3d_draw.cpp` | the D3D arm's redirected emission (phase C) |

These are `docs/reusability.md`'s **"never shared"** tier — engine reverse engineering and
hook addresses — and the numbers say why plainly. Of the **156 distinct `sub_XXXXXXXX`
addresses** referenced anywhere in the transplanted runtime, **155 do not exist as
functions in Case West's image**, so they are link errors: the good case.

**`sub_82475718` exists in BOTH images.** It would have linked silently and hooked a
completely unrelated function, inside a file whose entire purpose is to hook a specific
known one. That is a silent-wrong-execution trap of exactly the kind gotcha 5 exists to
prevent, and it is why these four moved out wholesale rather than being pruned address by
address. **One collision in 156 is not a small number when the failure mode is silent.**

Two of the four have an external API surface, so they are stubbed rather than deleted —
`cpu/debug_tunables_stub.cpp` and `gpu/d3d_draw_stub.cpp`. Both **report their own absence
once** rather than no-opping silently: a run whose navigation instruments are missing
should say so in its log, not look like a run whose instruments were present and found
nothing. `D3dDraw_Enabled()` returning false is the behaviour the disabled arm already
had, so nothing else needs to know.

## 2. The `CZ_` → `CW_` rename, and its gate

Renamed textually so **string literals were caught as well as identifiers** — `getenv("CW_…")`
is the whole mechanism, and an identifier-only rename produces an arm that reads as "no
effect" rather than "not wired". **222 distinct instrument names**, plus `cw_runtime`,
`cw_timebase`, `cw_file_selftest`.

Three classes the plain `sed` missed, all found by grepping for residue afterwards:

1. **`-DCZ_WINDOW` / `-DCZ_DEBUG_TUS`** — no word boundary after `-D`, so the *options*
   were renamed but their documentation and a build-time `message()` still told the user
   to pass a flag that no longer existed.
2. **Four user-visible strings** still saying "Case Zero": the smoke banner, the SDL
   window title, the window's FPS caption, and a debug log line. (Case Zero in *comments*
   was left alone — 66 of those legitimately refer to the sibling port.)
3. `DR2CZ-troubleshooting` paths → `DR2CW-`.

**The gate is not "it compiles"** (gotcha 151: an arm with no counter cannot be shown to
have engaged). Three-way, on `CW_DETERMINISTIC_CLOCK`, whose arm prints a line:

```
control  (no env set)            -> 0 lines          the arm is off
CW_DETERMINISTIC_CLOCK=1         -> prints           the arm engages
CZ_DETERMINISTIC_CLOCK=1         -> 0 lines          the OLD name does nothing
```

The third is the one that matters and is the reason the rename was requested: it proves a
stray `CZ_*` export on a machine running both ports cannot reach this binary.

## 3. Per-title constants: verified, not inherited

| constant | Case Zero | Case West | how |
|---|---|---|---|
| entry point | 0x825D9F30 | **0x825AC918** | XEX optional header |
| TLS slot count | 64 (`kTlsSize = 0x100`) | **64 — unchanged** | XEX `TLS_INFO` header, parsed |
| default stack | 0x40000 | **0x40000 — unchanged** | XEX `DEFAULT_STACK_SIZE` header |
| function imports | 244 | **247** | three-way, below |
| kernel variables | 13 | **13 — the same 13 ordinals** | A1's import dump |

The two "unchanged" rows are the point of the table: they were **read from this XEX**
rather than assumed, and the answer happening to match is a result, not a shortcut taken.

**The import counts are a three-way cross-check** — Xenia's A1 dump lists 247 `F` and 13
`V` entries, `ppc/ppc_recomp_shared.h` declares exactly 247 `__imp__` externs, and the
XEX's own descriptor walk agrees. The loader's runtime check was updated from 244/13 to
247/13 and **the first boot printed `resolved 247 function IAT slots + 13 kernel
variables` with no warning.**

The kernel *variable* set is the same 13 ordinals as Case Zero's — and as Asura's Wrath's,
across three studios, engines and SDK years. That is now the third independent
confirmation that the set is a property of the XDK rather than of the title. **Their
addresses moved**, though, which is exactly the sort of thing that transfers wrongly if
assumed; the table in `xex_imports.cpp` was re-transcribed from Case West's own A1 dump.

## 4. The three new imports

- **`NtCreateMutant` / `NtReleaseMutant`** — restored from the Asura's Wrath
  implementation (Case Zero had deliberately dropped the pair as dead code, since it does
  not import them). A **recursive** mutant with per-guest-thread ownership; releasing from
  a thread that does not own it returns `STATUS_UNSUCCESSFUL` rather than a silent
  success, so a guest that double-releases is visible.

  Not optional and not a stub: capture finding 3 measured **32,382 `NtReleaseMutant`
  calls in one solo gameplay session**. A mutant that fakes acquisition is gotcha 5's
  failure mode with a deadlock at the end of it, on the hot path, where it would be
  blamed on anything else.

- **`DbgBreakPoint`** — never called in either capture, which is the expected behaviour of
  a routine that fires only on a real debug break. Implemented anyway, and it **logs**:
  there is no return value to fake, so "fail honestly" here means continue execution and
  make sure the operator knows the guest asked to break. A silent no-op would turn the
  most informative event in a run into nothing at all. Not an abort — a debug break in a
  retail image is frequently non-fatal, and killing the process would lose everything
  after it.

`tools/gen_import_stubs.py` now reports **247 imports, 169 implemented, 78 stubbed**
(Case Zero: 244 / 166 / 78 — i.e. exactly the three new ones are implemented, not stubbed).

## 5. The link gate

```
230 translation units compiled with ZERO errors  (164 MB libppc_image.a, 1m36s wall)
cw_runtime --smoke:
  image  0x82000000..0x82B40000  (11.25 MB)
  code   0x82150000 + 0x88C4B8
  mapped 58695 entries, 0x82150000..0x829DBBA0
  OK: every generated symbol resolved and every mapping entry is sane.
```

Note the code range: `0x88C4B8` spans **both** code sections, and the last mapped function
starts at `0x829DBBA0`, inside `BINK`. The mapping covers the movie decoder, which is what
capture finding 14 says has to execute.

## 6. First boot — what it does and what it does not

`CW_NO_WINDOW=1 ./cw_runtime`, 60 s, **no crash** (exit 124 = the timeout, not a fault):

- resolves **247 + 13** imports with no warning;
- publishes the XEX headers and maps all 14 sections, correctly skipping `.reloc`, which
  runs past the loaded image buffer;
- brings up the XMA context array (320 contexts) and the decoder poll;
- mounts `game:` / `d:` and the save path;
- **opens 87 `.big` archives and 15 `.tex` through our VFS** — it is reading this title's
  real assets;
- makes **114 distinct kernel calls**;
- registers a graphics interrupt callback and receives **3,720 vblanks in 60 s (~62/s**,
  correct for 60 Hz), delivered to the guest's own `825B5B90`.

**What it does not do: present a frame.** It settles into a loop polling
`XamInputGetState`/`XamInputSetState` while a worker spins hard on
`RtlEnterCriticalSection`/`RtlLeaveCriticalSection` (6.8 M / 8.4 M hits in the minute).
That spin is the obvious next thread to pull, and it is where the next session starts.

**One consistency check worth recording:** the boot makes **zero** mutant calls and zero
`DbgBreakPoint` calls — which is exactly what capture A1 reports for the boot path, with
the mutants appearing only in A2's gameplay. Our runtime's boot and hardware's boot agree
on that, which is a small but real cross-check that the new imports are wired where they
belong rather than being called by accident.

## 7. Not done in W1

- **`kernel/content.cpp`'s save layer is untouched** and still carries Case Zero's title
  ID and save shape. It needs capture **A3** (the save round trip plus the physical save
  file), which is one of the two outstanding round-1 items.
- No attempt at the renderer yet — `assets/shader_spv/` holds 439 translated shaders
  waiting for it, and `CW_VKDRAW=1` has not been run.
- The four parked modules stay parked until Case West's own addresses are derived.
