# Player issue #12 — "Audio But Only White Screen" (2026-10-02)

Reporter joshiewowa, v1.1.1, Windows 11 25H2, **AMD Ryzen 7 4800H with Radeon
Graphics**, 32 GB. The game boots to the title (audio plays, the guest runs: vblanks,
shader loads, XLive sign-in, the KB/M splice) but the window stays white. The operator
reports the same symptom mostly from integrated-graphics players (Intel and AMD), and
has no iGPU to test on.

## What the capture establishes

- **The renderer failed before it named a device.** system.txt reads `gpu: unknown`,
  `driver: unknown`, `vulkan: 0.0.0`. `BugReport_SetGpu` is called from
  `CreateDevice` right after `QueryDeviceCaps`, and every earlier step is one of:
  `vkCreateInstance`, `vkEnumeratePhysicalDevices` (none listed), or no graphics queue
  family. So the failure is one of those three, the first two being the realistic ones.
  (This is not the "needs Vulkan 1.3" or "missing feature" path — those run after the
  GPU line is filled in.)
- **The report has no picture**, consistent with no frame ever being presented.
- **The log cannot say which step.** Its 60 s window starts at the guest's 445 MB
  `MmAllocatePhysicalMemoryEx`, which in a local full log is ~20 lines *after*
  `[vk] renderer UP` — bring-up had already happened (the player's first boot spent
  more than 15 s before that point). The `[vk]` lines are in the player's
  `cw_runtime.log`, which the bug report does not attach.

An absence in the log is a fact about the window, not about the renderer
(gotcha 25's shape).

## What was changed

1. `a25bb9a` — **the next report names the step.** The first bring-up failure is
   recorded in words (VK_CHECK and the explicit paths); system.txt gains
   `renderer: up | FAILED: <step> | not started`; a message box tells the player which
   step failed, the usual fix (vendor driver, both GPUs on a laptop), and where the log
   is. `CW_NO_RENDERER_DIALOG=1` suppresses the box.
2. `b90345b` — **candidate fix, unmeasured on the failing hardware.** On instance
   failure or zero devices only, retry with `DISABLE_LAYER_AMD_SWITCHABLE_GRAPHICS_1=1`
   (AMD's switchable-graphics implicit layer, shipped with every Radeon-iGPU Windows
   driver, with a public record of exactly this failure), then with
   `VK_LOADER_LAYERS_DISABLE=~implicit~` (overlay layers). A working machine is
   untouched. `CW_VK_NO_LAYER_RETRY=1` is the control arm.

**The hypothesis is from public knowledge, not from this capture** — the capture
bounds the step, it does not name the layer. The prediction: if it is an implicit
layer, the player's log says `THIS RUNG WORKED` and they get a picture; if not, the
`renderer: FAILED` line names the real step.

## Positive control (Linux, RTX 3070)

A fake implicit layer (`VK_ADD_IMPLICIT_LAYER_PATH`, enable variable set) whose
`vkCreateInstance` returns `VK_ERROR_INITIALIZATION_FAILED`:

| arm | result |
|---|---|
| no fake layer | no retry line, renderer UP, `renderer: up` |
| layer disabled by the AMD variable | rung 1 worked, renderer UP |
| layer with an unrelated disable variable | rung 1 fails, rung 2 worked, renderer UP |
| `CW_VK_NO_LAYER_RETRY=1` + F9 | `gpu: unknown`, `vulkan: 0.0.0` — **issue #12's exact report shape** — plus `renderer: FAILED: vkCreateInstance failed: VK_ERROR_INITIALIZATION_FAILED (-3)`; dialog raised |

## Owed

- The Windows compile (czwin) — `_putenv_s` and the dialog are untested there.
- A build to the reporter, or their `cw_runtime.log` from the current build
  (`%LOCALAPPDATA%\XenonLive\games\case_west\cw_runtime.log`), which already holds
  the `[vk]` lines that name the step.
- `cw_runtime --diag` does not have the retry; it is the instrument to hand a player
  whose renderer fails, so it may want the same ladder.
