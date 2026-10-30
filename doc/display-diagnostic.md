# Display-only diagnostic candidate

Base: `a9c8325c190ec4ea6cb60f16495ff3687be89ff6`; KWin 6.8.0, package release 1.3.
No microphone changes, Android APK changes, Xwayland changes, SELinux changes,
frame-dropping fixes, or protocol changes are included.

The compositor thread records metadata at the end of painting and around
presentation. `ANLAND_FRAME_DIAG` reports at most one summary per second plus
sixteen anomalous examples, automatically stopping after 300 seconds from the
first presentation. No pixels, keyboard input, window titles, addresses, or
application identities are logged. The additional bookkeeping/logging can affect
timing; this is not a performance benchmark.

Fields:
- `target`, `rendered_slot`: selected slot before handoff and last painted slot.
- `updated`: KWin included this layer in `layersToUpdate`.
- `painted`: endFrame ran since the preceding presentation attempt (even for an
  empty repaint region). `renders` counts those endFrame calls.
- `debt`: selected slot still owes accumulated damage at the observation point.
- `revision`: scene-change revision last painted into this slot. Catch-up painting
  without new scene damage retains the current scene revision.
- `previous`: maximum revision previously handed off, not physical scanout.
- `unpainted_debt`: presentation attempts without endFrame on a slot owing damage.
- `mismatches`: endFrame slot differs from the selected handoff slot.
- `regressions`: successful handoffs of a slot with an older tracked revision.
- `fence`/`unfenced`: whether a producer fence existed; NOT proof Android received it.
- `handed`/`accepted`: transport accepted the frame, NOT proof of GPU completion.

Session replacement clears slot/revision tracking, but retains cumulative counts
and the original logging time limit. A graphics error can reset buffers during
handoff; correlate such records with reconnect/fallback logs.

Do not interpret zero counters as proof that all downstream frame ordering is
correct. Android truncation handling and SurfaceFlinger scanout remain separate.

Check without hardware:

```sh
c++ -std=c++17 -Wall -Wextra -Werror tests/frame_diagnostic_test.cpp -o /tmp/frame-diagnostic-test
/tmp/frame-diagnostic-test
```

Install only a verified complete package with ordinary pacman checks after KWin
has exited, retaining the exact original rollback package. This candidate must
not replace the user's stock rollback archive or alter the parallel microphone
candidate package. Stop the diagnostic session and reinstall stock afterward.
