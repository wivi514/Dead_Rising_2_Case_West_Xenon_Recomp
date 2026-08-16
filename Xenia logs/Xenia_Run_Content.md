# Dead Rising 2: Case West — Xenia capture run index

One entry per run. Requests: `docs/xenia-capture-requests.md` (round 1).

All captures run on the operator's **Windows** PC on the instrumented **xenia-canary
fork** (Xenia is unstable on the Linux box this repo lives on). Record the fork's
`Build:` line per run rather than assuming it is unchanged.

The STFS **package** is launched directly —
`…\58410B00\000D0000\D01128ABB9C7F9694DAE26AC591A269F8480E85A58` — **not** the extracted
`default.xex`. Title ID **58410B00**. (Case Zero is `58410A8D`; the folders look alike.)

**This file is tracked in git; the captures themselves are not** (`Xenia logs/*` is
gitignored with this file negated out). So this index is the only thing that survives if a
capture is lost, and an unindexed capture is a file nobody can interpret in three weeks.

---

## Entry template

Copy this per run.

```
### <id> — <one-line what it is>  → `<directory>/`
**Delivered <date>.** <what was done, in order. Say what was skipped, or that nothing was.>
Flags: log_level=?, flush_log=?, high_freq=?, dump_shaders=?, trace_* =?, resolution=?
Xenia build: <the `Build:` line>
Clean exit: <did the log end with `Cheap-skate exit!`?>
license_mask / trial: <what XamContentGetLicenseMask returned; any unlock prompt?>

- <findings, surprises, anything that looked wrong IN XENIA ITSELF>
- <anything this run shows that the request did not predict — this is the most
  valuable part of an entry, and two of Case Zero's round-1 premises were refuted
  exactly here>

Files: <names and sizes>
```

---

## Round 1

**Nothing captured yet.** Round 1 was requested 2026-08-15 (session 1) and is open.
See `docs/xenia-capture-requests.md`; A1 is to be delivered alone, first.
