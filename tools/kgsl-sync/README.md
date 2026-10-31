# Private KGSL implicit-sync diagnostic (not a released fix)

Pinned base: lfdevs/mesa-for-android-container
`98f3d6229d61452cef80f8563af7c56ae599dc14`. Its Arch release libgallium
matches the installed native driver byte-for-byte. `mesa.patch` is opt-in with
`ANLAND_KGSL_IMPLICIT_SYNC=1` and is intended only for a reversible private
compositor/Xwayland session. Do not install over system Mesa.

The patch exports current dependencies for shared imported BOs, combines them
with the existing explicit input fence, submits that dependency to KGSL, and
publishes the resulting completion fence back to every shared BO. Earlier
merged-submit BO references survive through publication. Shared accesses are
conservatively RW (less reader parallelism, simpler ordering). CPU preparation
also waits for external dependencies. Device-level asynchronous submission and
the explicit-sync heuristic are disabled only in the opt-in mode so shared
fences are published before the flush returns. GPU execution remains async.

This is **not** an inter-process lock: DRI3/Wayland ownership must exclude
conflicting access throughout export/submit/publication. Cache visibility,
release order, frontend batching and actual screen behavior remain to test.
The submission callback's parent ignores error returns, so an incomplete
handoff explicitly aborts this diagnostic process rather than falsely proceeding.
A session-level watchdog/stock rollback is required before live activation.

`test-submit.c` compiles the real patched submission body with test-only Mesa
scaffolding and real KGSL/Linux UAPI layouts. It checks all 16 combinations of
mode/shared/output/input flags, BO lifetime through merged submission, caller FD
ownership/leaks, and fail-closed export/merge/submit/timestamp/publication errors.
These are mock tests, not GPU correctness or visual validation.

CI builds only private freedreno/softpipe libgallium, without LLVM or Vulkan
replacement. The SONAME/version is set to the installed `26.3.0-arch1.1` because
stock EGL/GLES also depend on this DSO's shared GLAPI. Both a private
`LD_LIBRARY_PATH` and `LIBGL_DRIVERS_PATH` are required; merely overriding one can
load both old and new GLAPI implementations. Do not propagate these overrides to
Steam/FEX or the user-manager environment. Confirm loaded paths before input.
No final game/compositor test has passed yet.
