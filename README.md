# Dead_Rising_2_Case_West_Xenon_Recomp

A **XenonRecomp**-based static recompilation of the Xbox 360 XBLA title
**Dead Rising 2: Case West** (Capcom / Blue Castle Games, 2010).

This is the **fourth** game ported with this pipeline in this workspace, and the first
whose immediate predecessor is a near-complete port of the *same engine*:

- `~/GithubRepo/Dead_Rising_2_Case_Zero_Xenon_Recomp` — **Dead Rising 2: Case Zero.**
  Same engine, same studio, same year, same container, same compiler. ~46 parts deep.
  This port is a transplant of it plus a delta.
- `~/GithubRepo/Fable2XenonRecomp` — the original, and the deepest: 91k functions to a
  live rendered world, with the whole methodology in its `CLAUDE.md` and `docs/`.
- `~/GithubRepo/Asuras_Wrath_Xenon_Recomp` — the second, which proved the template
  transfers and added a numbered findings ledger plus transferable gotchas.

**Multiplayer / co-op is out of scope for now.**

## How much of Case Zero transfers — measured, not assumed

| | Case Zero | Case West |
|---|---|---|
| container | STFS `LIVE` arcade title | same — reader worked unchanged |
| XEX key / compression | devkit (all-zero) key, LZX | same |
| image base / size | `0x82000000` / `0xB40000` | identical |
| `.text` | `0x82150000 + 0x873564` | `0x82150000 + 0x87BC54` (+2%) |
| **code sections** | **1** | **2 — `.text` and `BINK`** |
| kernel imports | 244 | **247 — a strict superset, +3** |
| `.big` archive format | cracked | reads unchanged, first try |
| jump tables | 232 / 6,114 labels | 205 / 5,791 labels |
| functions | 57,822 | 58,345 |
| Bink video | **none** (retracted after measurement) | **yes** — 4 XEX sections, 10 `.bik`, 66 MB |

The import table being a strict superset is the decisive one: every kernel and XAM
import Case Zero implements, Case West also needs, and it needs exactly three more
(`DbgBreakPoint`, `NtCreateMutant`, `NtReleaseMutant`).

## Status — session 1 (2026-08-15), bootstrap

- [x] STFS package unpacked — 305 files, 1.22 GB, `default.xex` + `data/`.
- [x] XEX identity established: title `58410B00`, base `0x82000000`, entry `0x825AC918`,
      image `0xB40000`, `.text 0x82150000 + 0x87BC54`, `BINK 0x829CBE00 + 0x106B8`.
- [x] Save/restore helper ladders located and cross-checked (all 8, contiguous vector
      ladders).
- [x] **205 jump tables recovered** (107 absolute, 62 offset8, 36 offset16, 5,791 case
      labels) **across both code sections** — XenonAnalyse finds zero on this compiler.
- [x] **Recompilation succeeds** — 58,345 functions → 156 MB of C++ across 228 TUs, with
      **zero** `jump outside function` errors and **zero** dropped direct branches.
- [x] **Unlowered-switch gate passes** — 0 defects; the 2 switch-shaped `bctr` sites not
      in the table are benign frameless thunks.
- [ ] 39 unrecognized-instruction sites (6 mnemonics) + 20 `float16_4` pack sites — open.
- [x] Kernel import surface censused and diffed against Case Zero.
- [ ] Nothing compiled by a C++ compiler yet.
- [ ] **No Xenia ground-truth capture of this title exists.**
- [ ] No runtime.

Next steps are `docs/port-plan.md`, items W0–W8. Day-1 evidence is
`docs/bootstrap-2026-08-15.md`.

## Reproducing the bootstrap

The game data is not distributable and is not in this repository. With the STFS package
placed at `assets/package/58410B00/000D0000/<hash>`:

```
python3 tools/extract_stfs.py "assets/package/58410B00/000D0000/<hash>" -o assets/game
./tools/build_xex_image_dump.sh
./tools/xex_image_dump assets/game/default.xex assets/game/default_image.bin
mkdir -p ppc && cd config && ~/GithubRepo/XenonRecomp/build/XenonRecomp/XenonRecomp \
    CaseWest.toml ~/GithubRepo/XenonRecomp/XenonUtils/ppc_context.h
```

XenonRecomp must be the locally patched build — stock upstream hardcodes the retail AES
key and returns an empty image, with no diagnostic, on this title's devkit-key XEX.

Full command reference, including the config-regeneration gates, is in `CLAUDE.md`.
