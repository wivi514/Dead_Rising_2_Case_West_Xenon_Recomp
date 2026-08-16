#!/usr/bin/env python3
"""Diff our runtime's kernel-call sequence against a Xenia capture. The phase gate.

Why this exists
---------------
Every phase in docs/runtime-plan.md is gated on "our kernel-call sequence matches
Xenia's", never on "it seems to work". Without a script that comparison is an
eyeball claim over a 13.9 MB log and a growing one of our own, and it silently
stops being done. This makes the gate a command.

What is compared, and why it is FIRST-OCCURRENCE order
------------------------------------------------------
Not the raw call stream. The stream is dominated by polling — Case Zero calls
XamInputGetCapabilities 5,501 times and XamUserGetXUID 4,441 times in the A1 boot
alone — and our thread interleaving will never match an emulator's cycle-by-cycle,
so a stream diff would be noise. The order in which each kernel entry point is
FIRST touched is a real, reproducible property: it is the order the title brings
its subsystems up, and a divergence in it means we steered the guest down a
different path.

THE FIRST TRAP: Xenia does not log every kernel call, even at log_level=3
-------------------------------------------------------------------------
Exports tagged `kHighFrequency` are suppressed unless the cvar
`log_high_frequency_kernel_calls=true`, which defaults to FALSE — and A1, A2, A3
and A4 all record it as false in their own config dumps. Anyone diffing a runtime
log against A1 without knowing this sees our runtime "calling functions Xenia never
called" and goes hunting for a divergence that does not exist.

The hidden set below is MEASURED ON THIS TITLE (part 2, 2026-08-15), and the
derivation is not the obvious one — see THE SECOND TRAP. There are 35 names, and
they are most of the synchronisation surface: RtlEnterCriticalSection, KeSetEvent,
NtWaitForSingleObjectEx, KeDelayExecutionThread, NtReadFile, VdSwap.

Those names are masked out of OUR side by default, and the script reports how many
it masked. `--include-high-frequency` unmasks them, for use against A5.

THE SECOND TRAP: `A5 - A1` IS NOT THE HIDDEN SET
-------------------------------------------------
That subtraction is what this script shipped with, inherited from Case Zero, and on
Case West it is wrong in BOTH directions — because it conflates two different
reasons a name can be missing from A1:

    (a) Xenia suppressed it            <- what we want to mask
    (b) that drive never went there    <- a real path difference, must NOT be masked

On this title `A5 - A1` is 58 names and 23 of them are case (b): A5's drive entered
the storage-device selector and the XMP/XMsg paths that A1's did not, dragging in
XamShowDeviceSelectorUI, XamContentAggregateCreateEnumerator, XamTaskSchedule,
XMsgInProcessCall, XamAlloc, NtCreateMutant and friends. Masking those would hide
exactly the class of divergence this script exists to catch — the storage path is a
place a boot can hang.

The discriminator that actually separates (a) from (b): a suppressed name can never
appear in ANY hf=false capture, however far that drive got. So the hidden set is

    first-occurrence(A5)  -  union(every capture whose log head says hf=false)

which uses all ten other captures instead of one. Regenerate it with `--derive-mask`,
which reads the flag out of each log head rather than trusting a sidecar config or
this comment.

Note the direction the old subtraction was wrong in as well: it MISSED
XMAReleaseContext, which is genuinely suppressed here. An unmasked hidden name shows
up as a spurious ours-only divergence — the same false alarm, from the other side.

A1 remains the authority for the boot sequence, and A5 only for the region its drive
covers: five names in the wider union (NtWriteFile, XamContentCreateEx,
XamContentGetDeviceData, XamShowSigninUI, XGIUserWriteAchievements) are absent from
A5 entirely. Neither capture alone is the gate. Run both.

THE THIRD TRAP: Xenia's internal helpers wear the same line shape
------------------------------------------------------------------
`GetProcAddressByOrdinal`, `ResolvePath`, `SetThreadName`, `SetInterruptCallback`,
`XEnumerateCrossTitle`, `XamGetPrivateEnumStructureFromHandle` and
`XGIUserSetContext` are all logged by A1 in exactly the `Name(args)` shape a kernel
call uses, and none of them is an import of this image. Both sides are therefore
intersected with the image's real import set, read from `ppc/ppc_recomp_shared.h`
so it cannot drift (gotcha 10). A1's own import dump independently agrees on that
set: 244 `F` entries and 13 `V` entries.

And the line-prefix trap (gotcha 24): A1 carries `G>` `d>` `A>` `!>` `i>` `F>` `K>`
prefixes and unprefixed continuations. Filtering on `d>` alone silently loses whole
exports — `VdSwap` is logged at `i>`. The pattern below accepts every prefix.

Usage
-----
    python3 tools/kernel_call_diff.py \\
        --xenia "Xenia logs/A1_boot_title_fullgame/cz_run1.log" \\
        --ours  /tmp/run.log

    # only the CRT bring-up
    python3 tools/kernel_call_diff.py ... --limit 20

    # the STRONG gate: the synchronisation surface too, against A5
    python3 tools/kernel_call_diff.py \\
        --xenia "Xenia logs/A5_highfreq_boot/cz_run5.log" \\
        --ours /tmp/run.log --include-high-frequency

Two different verdicts, on purpose. Masked, ANY mismatch fails: those names are
subsystem bring-up and their order is reproducible. Unmasked, a run of mismatched
positions holding the same SET of names passes — it records which thread won a
race, and no runtime reproduces an emulator's scheduling.
"""

import argparse
import gzip
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SHARED_H = REPO / "ppc" / "ppc_recomp_shared.h"

# Xenia's level-3 kernel-call lines look like:
#   d> F8000008 NtAllocateVirtualMemory(7018FB90(00000000), ...)
# The leading token is the log level letter, then the thread id. Every letter is
# accepted deliberately — see gotcha 24 in the docstring.
XENIA_CALL = re.compile(r"^[A-Za-z!]> [0-9A-F]+ ([A-Za-z][A-Za-z0-9_]*)\(")

# Our side: kernel/klog.h emits exactly one of these the first time each import is
# called. Repeats use the [kcall+] prefix and are ignored here by construction.
OURS_CALL = re.compile(r"^\[kcall\] (\w+)$")

# Exports Xenia tags kHighFrequency THAT CASE WEST ACTUALLY USES.
#
# Measured on this title's own captures on 2026-08-15 (part 2) with `--derive-mask`,
# which is the reproduction: A5's names minus the union over all ten hf=false
# captures. Do NOT regenerate this as `A5 - A1` — that is the inherited derivation and
# THE SECOND TRAP in the docstring explains why it is wrong in both directions.
#
# Deriving it from the captures rather than from a Xenia checkout means it is
# automatically the intersection with what this title imports, and it stays true to
# the fork the captures were taken on rather than to whatever Xenia is today.
#
# One name is here on a weaker footing than the rest, and is marked: NtSetInformationFile
# is imported by this image but appears in NO capture at all, A5 included. So it is
# either suppressed-and-never-called or simply never called, and the captures cannot
# tell those apart. It is masked because either way Xenia's side shows nothing, so
# leaving it visible could only ever manufacture an ours-only divergence.
XENIA_HIGH_FREQUENCY = {
    "KeAcquireSpinLockAtRaisedIrql", "KeDelayExecutionThread", "KeEnterCriticalRegion",
    "KeGetCurrentProcessType", "KeLeaveCriticalRegion", "KeQueryPerformanceFrequency",
    "KeReleaseSpinLockFromRaisedIrql", "KeSetEvent", "KeTlsGetValue",
    "KeWaitForMultipleObjects", "KeWaitForSingleObject", "KfAcquireSpinLock",
    "KfReleaseSpinLock", "MmQueryAddressProtect", "MmQueryStatistics", "NtClearEvent",
    "NtReadFile", "NtReleaseSemaphore", "NtSetEvent",
    "NtWaitForMultipleObjectsEx", "NtWaitForSingleObjectEx", "RtlEnterCriticalSection",
    "RtlInitializeCriticalSectionAndSpinCount", "RtlLeaveCriticalSection",
    "RtlNtStatusToDosError", "RtlTryEnterCriticalSection", "VdSwap",
    "XamContentGetLicenseMask", "XamInputGetState", "XamUserGetSigninState",
    "XAudioGetVoiceCategoryVolume", "XAudioSubmitRenderDriverFrame", "XMACreateContext",
    "XMAReleaseContext", "XNotifyGetNext",
    # never observed in any capture, including A5 — see the note above
    "NtSetInformationFile",
}


def image_imports() -> set[str]:
    """The kernel imports this image actually references (the recompiler's own list)."""
    names = set(re.findall(r"PPC_EXTERN_FUNC\(__imp__(\w+)\);", SHARED_H.read_text()))
    if not names:
        sys.exit(f"no __imp__ externs found in {SHARED_H} — has ppc/ been generated?")
    return names


def first_occurrences(lines, pattern, keep) -> list[str]:
    seen, order = set(), []
    for line in lines:
        m = pattern.match(line.rstrip("\n"))
        if not m:
            continue
        name = m.group(1)
        if name in keep and name not in seen:
            seen.add(name)
            order.append(name)
    return order


def mismatch_windows(xenia, ours):
    """Split the common prefix into maximal runs where the two disagree, and test each
    run for SET equality.

    Element-wise equality is the right test for the masked sequence — those names are
    subsystem bring-up and their order is a real, reproducible property. It is the
    WRONG test once kHighFrequency names are unmasked, because those names are the
    lock and wait paths: which of `KeSetEvent` / `NtWaitForSingleObjectEx` is touched
    first records which thread won a race, and no runtime reproduces an emulator's
    scheduling. Only a run whose sets actually differ is evidence of anything.

    Returns (start, end, xenia_only, ours_only).
    """
    windows = []
    n = min(len(xenia), len(ours))
    i = 0
    while i < n:
        if xenia[i] == ours[i]:
            i += 1
            continue
        start = i
        while i < n and xenia[i] != ours[i]:
            i += 1
        a, b = set(xenia[start:i]), set(ours[start:i])
        windows.append((start, i, sorted(a - b), sorted(b - a)))
    return windows


def report_windows(windows):
    if not windows:
        return
    print("\nWindow analysis — a run of mismatched positions whose two sides hold the")
    print("SAME SET of names is a scheduling permutation, not a divergence. Only")
    print("'REAL' rows are evidence.\n")
    for start, end, xonly, oonly in windows:
        if not xonly and not oonly:
            verdict = "PERMUTATION (same set)"
        else:
            verdict = f"REAL: xenia-only={xonly} ours-only={oonly}"
        print(f"  [{start + 1:>3}..{end:>3}]  {end - start:>2} entries  {verdict}")
    real = [w for w in windows if w[2] or w[3]]
    print(f"\n{len(windows)} mismatch window(s): {len(windows) - len(real)} permutation, "
          f"{len(real)} real.")
    if not real:
        print("SET MATCH: every mismatch is a permutation. Exit 0.")


HF_FLAG = re.compile(r"log_high_frequency_kernel_calls\s*=\s*(\w+)")


def derive_mask(logdir: Path) -> int:
    """Re-derive XENIA_HIGH_FREQUENCY from the captures, and diff it against the constant.

    This exists because the constant above arrived from Case Zero with Case Zero's
    derivation baked in, and was wrong on this title by one name in each direction.
    A constant nobody can regenerate goes stale silently; this makes it a command.

    The hf flag is read from each log's OWN head, not from the sidecar .toml — only 3
    of the 11 captures ship a config, and a sidecar can be the wrong run's.
    """
    logs = sorted(logdir.glob("*/xenia_*.log.gz")) + sorted(logdir.glob("*/xenia_*.log"))
    if not logs:
        sys.exit(f"no xenia_*.log[.gz] under {logdir}")

    visible: set[str] = set()   # names logged by a capture that was NOT hiding them
    hidden_capable: set[str] = set()   # names seen in an hf=true capture
    n_true = n_false = 0
    for log in logs:
        opener = gzip.open if log.suffix == ".gz" else open
        names, flag = set(), None
        with opener(log, "rt", encoding="utf-8", errors="replace") as f:
            for line in f:
                if flag is None:
                    m = HF_FLAG.search(line)
                    if m:
                        flag = m.group(1).lower() == "true"
                m = XENIA_CALL.match(line.rstrip("\n"))
                if m:
                    names.add(m.group(1))
        if flag is None:
            sys.exit(f"{log}: no log_high_frequency_kernel_calls line — cannot classify it")
        if flag:
            n_true += 1
            hidden_capable |= names
        else:
            n_false += 1
            visible |= names
        print(f"  {log.parent.name:<24} hf={str(flag):<5} {len(names):>4} distinct names")

    if not n_true:
        sys.exit("no capture has log_high_frequency_kernel_calls=true; cannot derive")

    derived = hidden_capable - visible
    print(f"\n{n_false} hf=false capture(s) -> {len(visible)} visible names")
    print(f"{n_true} hf=true capture(s)  -> {len(hidden_capable)} names")
    print(f"derived hidden set: {len(derived)} names\n")
    for n in sorted(derived):
        print(f"    {n}")

    # The constant may legitimately hold names the captures cannot see at all (see the
    # NtSetInformationFile note); those are reported, not treated as a failure.
    unseen_anywhere = (XENIA_HIGH_FREQUENCY - derived) - visible
    stale = (XENIA_HIGH_FREQUENCY - derived) & visible
    missing = derived - XENIA_HIGH_FREQUENCY
    print()
    if unseen_anywhere:
        print(f"in the constant but in NO capture (harmless, kept): {sorted(unseen_anywhere)}")
    if stale:
        print(f"STALE — masked here but VISIBLE to Xenia on this title: {sorted(stale)}")
    if missing:
        print(f"MISSING — suppressed on this title but not masked: {sorted(missing)}")
    if stale or missing:
        print("\nThe constant does not match the captures. Update XENIA_HIGH_FREQUENCY.")
        return 1
    print("Constant agrees with the captures.")
    return 0


def read_lines(path: Path):
    # Xenia logs carry occasional non-UTF-8 bytes from guest strings.
    with path.open("r", encoding="utf-8", errors="replace") as f:
        yield from f


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--xenia", type=Path, help="Xenia log_level=3 capture")
    ap.add_argument("--ours", type=Path, help="our runtime's stderr log")
    ap.add_argument("--derive-mask", type=Path, metavar="LOGDIR", nargs="?",
                    const=REPO / "Xenia logs",
                    help="re-derive XENIA_HIGH_FREQUENCY from every capture under "
                         "LOGDIR and diff it against the constant; exit 1 if they "
                         "disagree. Does not run a comparison.")
    ap.add_argument("--limit", type=int, default=0,
                    help="compare only the first N entries")
    ap.add_argument("--include-high-frequency", action="store_true",
                    help="do not mask kHighFrequency names (use with A5, the one "
                         "capture taken with log_high_frequency_kernel_calls=true)")
    args = ap.parse_args()

    if args.derive_mask is not None:
        sys.exit(derive_mask(args.derive_mask))
    if not args.xenia or not args.ours:
        ap.error("--xenia and --ours are both required (or use --derive-mask)")

    imports = image_imports()
    visible = imports if args.include_high_frequency else imports - XENIA_HIGH_FREQUENCY

    xenia = first_occurrences(read_lines(args.xenia), XENIA_CALL, visible)
    ours_all = first_occurrences(read_lines(args.ours), OURS_CALL, imports)
    ours = [n for n in ours_all if n in visible]
    masked = len(ours_all) - len(ours)

    if args.limit:
        xenia, ours = xenia[: args.limit], ours[: args.limit]

    print(f"xenia : {args.xenia}  -> {len(xenia)} distinct calls (visible set)")
    print(f"ours  : {args.ours}  -> {len(ours)} distinct calls "
          f"({masked} masked as kHighFrequency, invisible to Xenia)")
    print()

    width = max((len(n) for n in xenia + ours), default=10) + 2
    divergence = None
    for i in range(max(len(xenia), len(ours))):
        x = xenia[i] if i < len(xenia) else ""
        o = ours[i] if i < len(ours) else ""
        # Running out of OUR calls is not a divergence — it is how far we got, and
        # every phase before the last is expected to stop early. Running past the end
        # of Xenia's, or disagreeing on a name, is.
        if x == o or (o == "" and x != ""):
            mark = "  " if x == o else ".."
        else:
            mark = "!!"
            if divergence is None:
                divergence = i
        print(f"{mark} {i + 1:4d}  {x:<{width}} {o}")

    print()
    if divergence is None:
        if len(ours) < len(xenia):
            print(f"PREFIX MATCH: our {len(ours)} calls are an exact prefix of Xenia's "
                  f"{len(xenia)}. We stopped before '{xenia[len(ours)]}'.")
        else:
            print("MATCH: identical first-occurrence sequences.")
        sys.exit(0)

    print(f"DIVERGENCE at position {divergence + 1}: "
          f"xenia={xenia[divergence] if divergence < len(xenia) else '<end>'} "
          f"ours={ours[divergence] if divergence < len(ours) else '<end>'}")

    # The permutation argument applies to kHighFrequency names, which are the
    # lock/wait paths. It must NOT relax the masked gate: there a reordering of the
    # subsystem bring-up sequence is exactly the divergence this script exists to
    # catch, so the masked run still fails on any mismatch at all.
    if not args.include_high_frequency:
        sys.exit(1)

    windows = mismatch_windows(xenia, ours)
    report_windows(windows)
    sys.exit(1 if any(w[2] or w[3] for w in windows) else 0)


if __name__ == "__main__":
    main()
