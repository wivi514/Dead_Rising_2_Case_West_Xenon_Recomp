# Part 14 kickoff — final update round and build handoff

Written 2026-10-14. This is the live handoff; `part13-kickoff.md` is superseded.

## Current state

* Branch: `xlive-integration`. The v1.1.1 tag is at `e05460c`; the current source was
  already 43 commits beyond that tag before this part. The four v1.1.1 artifacts staged
  under `~/Release/Case West/1.1.1/` are therefore stale for this tree.
* The October 3 Case Zero fix round is recorded in `docs/imported-fixes.md` §15.
  The October 9–14 runtime/toolkit and renderer/macOS/Windows/detail updates are in the
  branch history. The final candidate from that pass, Case Zero `78632f1`, applies to
  Case West's `zombie_security.big` and `zombie_soldier.big` and is recorded in §16.
* §16's classifier wrapper is implemented at Case West `sub_8273B160`, derived as a
  unique 1.000 instruction-shape match of Case Zero `sub_827A2E10`. It changes only
  `chestupper_neck` and `headwear` names with a misplaced variant digit. The similar
  `geo_lodShape4Deformed` name is intentionally untouched.

## Work still owed

1. Build the updated source and run the applicable release gates. Do not use the v1.1.1
   staged artifacts as outputs for this source revision.
2. On the built runtime, use the DebugJump route with the default and
   `CW_ZOMBIE_NAME_FIX=0`. Confirm the body-part hide-mask entries follow the variants
   and the control reproduces the original classification. The asset-name census is
   established; the runtime mask values and visible effect are not yet measured.
3. Continue the final release checklist after the build gates. No new release artifacts
   were built in part 14, per the operator's instruction to finish the update first.

## Relevant records

* `docs/imported-fixes.md` §15–16 — the October 3 import and this final candidate.
* Case Zero `docs/final-state.md` — final v1.1.3 changes and the sibling's release
  handoff. Case West imported applicable changes selectively; title-specific/co-op
  features remain governed by the decisions recorded in `imported-fixes.md`.
* Claude's Case West memory directory: `/home/wivi514/.claude/projects/`
  `-home-wivi514-GithubRepo-Dead-Rising-2-Case-West-Xenon-Recomp/memory/`.
