# Part 9 kickoff — the live hand-off

**Written at the end of part 8 (2026-09-03). THIS IS THE LIVE ONE.**
`part8-kickoff.md` is superseded; its §2 (implement Case Zero's new research) ran to
completion in one session.

Read this, then `docs/imported-fixes.md` §5 (the import row — what came, its controls,
and the local verdicts) and `docs/native-kbm-import.md` (the address re-derivation).

---

## 1. WHAT PART 8 WAS: Case Zero parts 83-93, imported and verified

One session. Two commits: `85324f8` (the import) and `074e262` (the key-cap prompt
overlay). Everything the operator asked for — performance, real MSAA, keyboard/mouse,
"and more" — is in, boot-verified, and **OPERATOR-ACCEPTED IN PLAY** ("All good", 2026-09-03,
one sitting: keyboard+mouse gameplay, mouse-look, MSAA 2x, key-cap prompts with
device-follow — 23/23 glyphs located and swapped live — save loaded from the new OS
location, zero faults in the session log). Per-item states live in imported-fixes §5,
not here, so they cannot go stale in two places.

The method's new step, worth keeping: **when the sibling's work supersedes local work,
revert the local work BEFORE the three-way merge.** With our part-7 record stack in
the "ours" file the renderer merge had 32 conflicts; with it reverted first, ZERO.
The part-7 stack is parked (`gpu/parallel_record.cpp` kept, unbuilt); its controls
(`CW_VK_NO_SECONDARIES` / `CW_VK_NO_DEFER_RECORD` / `CW_VK_PREC_EXEC`) are GONE —
the serial control is now `CW_VK_NO_PAR_RECORD=1`.

### The picture-bisection order (their standing rule, adopted)

Any picture complaint bisects on the newest command-buffer-rearranging default first:
1. `CW_VK_MSAA=1` (single-sample control — 2x is the default now)
2. `CW_VK_NO_PAR_RECORD=1`
3. `CW_VK_NO_DEFERRED_CLEAR=1` (then `CW_VK_DEFER_FULL_RECT=1` to split scoping from
   ordering)
4. `CW_NO_KB_PROMPTS=1` / `CW_NO_PATCHED_ASSETS=1` for anything glyph/text-shaped.

### Local soak verdict (the recorder A/B)

See imported-fixes §5's addendum — filled from `tools/cw_trace_band.py` over the
part-8 chains (`p8_parrec_on` / `p8_parrec_off`, binary sha `8eb792bfb41bd48e`).

## 2. WHERE PART 9 STARTS

**Ask the operator.** The obvious candidates, none pre-chosen:

1. **Operator play space** — KB/M feel (the five Case Zero fix rounds all came from
   operator sittings; expect the same here), MSAA-2x acceptance at their resolution,
   the live-resolution X-apply, prompt-icon look, save-location move. The parked
   minor-visual-issues list is still THEIRS to name (§2b of part8-kickoff stands).
2. **The soak follow-ups** if the recorder verdict wants tuning
   (`CW_VK_RECORD_CHUNK`, worker share — their step-0 pricing instruments
   `CW_VK_RESOLVE_SPLIT_CENSUS` came along).
3. **Watch the sibling** — their uncommitted shadow-distance work
   (`shadow_distance.{cpp,h}`) and `96c3b95`'s property censuses will land; that's
   the next import row. The game-side FOV (`camera_fov.cpp`) recipe also transfers
   when someone re-derives this title's camera addresses.
4. **The Windows leg** — win_compat.h and the translator's dxcapi plumbing came
   along; their CI scripts and packaging did not. Relevant when release becomes the
   subject here, as it did there.

## 3. THE BACKLOG OTHERWISE (carried; part-8 deltas noted)

1. Game-side FOV — recipe transfers, addresses do not (unchanged).
2. ~~`find_dropped_branches.py` — the owed W0 gate~~ — **RUN 2026-09-03: "No dropped
   branches."** Zero in both classes, so no --prune/--widen follow-up exists. The
   capable-of-failing proof (gotcha 30) rests on the sibling lineage, where this
   tool caught real drops; a local break-it-on-purpose run remains cheap if anyone
   wants the gate airtight.
3. Eight unconsumed B4 capture frames (unchanged).
4. The F3/DebugEnter boot-logo crash (parked by the operator).
5. The shader-bank completeness test — SOFTENED: the in-process translator now
   translates misses at first sight, so an incomplete bank self-heals per boot; the
   committed bank still grows only via dumps.
6. The parallel-record module's export to Case Zero — NOTE: their part-89 recorder
   made our module's export moot; what still exports is finding 69's 2a/2b
   restructure lesson, already recorded in their part-87 exchange.
7. Key-cap icons: `assets/game_kbm/` regenerates via `python3 tools/gen_kbm_icons.py`
   (gitignored output; the tool + gates are committed).

## 4. MEASUREMENT RULES (the part-7 set, plus part 8's lessons)

* Arms in one display state, back-to-back; only accepted runs; verdicts only through
  `cw_trace_band.py`; **one replay chain at a time — and NO cw_runtime launches of
  ANY kind while a chain runs**: part 8 lost seven soak attempts to its own
  verification boots tripping the already-running guard (the guard worked; the
  session cost was real).
* NO BUILDS while a soak chain runs; the log's `sha=` field is the audit.
* Headless recipes that do not set `CW_VK_MSAA` now run at 2x — set `CW_VK_MSAA=1`
  for any comparison against pre-part-8 numbers.
* A shape-match at loose masking is a candidate, not a finding — the conversion-hook
  mis-match (docs/native-kbm-import.md, "one trap actually hit") is the example.
