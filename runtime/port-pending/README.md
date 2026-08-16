# Per-title reverse engineering, awaiting re-derivation for Case West

These four modules came across in the W1 transplant and are **excluded from the build**.
They are not dead code and they are not broken — they are Case Zero's *engine reverse
engineering*, which `docs/reusability.md` classes as **"never shared"** along with hook
addresses and renderer translation specifics.

Between them they reference **89 hardcoded `sub_XXXXXXXX` guest addresses**. Of those,
**155 of 156** distinct addresses referenced anywhere in the transplanted runtime do not
exist as functions in Case West's image at all — so they are link errors, which is the
good case.

**The bad case is the one that made this a decision rather than a chore: `sub_82475718`
exists in BOTH images.** It would have linked silently and hooked a completely unrelated
function, in a file whose whole purpose is to hook a specific known one. That is a
silent-wrong-execution trap of exactly the kind gotcha 5 exists to prevent, and it is why
these files are moved out wholesale rather than pruned address by address.

| file | what it is | what it needs before it can come back |
|---|---|---|
| `debug_tunables.cpp` | the debug-menu / DebugJump / AutoChuck navigation toolchain (29 hooked addresses) | Case West's own debug-menu entry points. The retail image carries the debug build again (`COMMAND_RENDERDEBUGMENU`, `DontAutoCompleteOnDebugJump`), so the equivalent exists — it just has to be found. `docs/port-plan.md` W8. |
| `guest_probe.cpp` | argument probes on named guest functions (48 addresses) | nothing structural; point it at Case West functions when there is a value to trace. |
| `d3d_hooks.cpp` | the D3D translation arm's hook table (phase A) | Case West's D3D function addresses. |
| `d3d_draw.cpp` | the D3D translation arm's redirected emission (phase C) | as above. |

The two modules with an external API surface are stubbed so the rest of the runtime still
links and still *reports* their absence rather than silently doing nothing:
`cpu/debug_tunables_stub.cpp` and `gpu/d3d_draw_stub.cpp`. Delete the stub when you
restore the real file.

**Do not delete these.** Recovering the technique is most of the value; only the addresses
are wrong.
