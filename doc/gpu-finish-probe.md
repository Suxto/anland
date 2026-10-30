# GPU finish synchronization probe

This temporary KWin 6.8.0-1.4 package is based on
`a9c8325c190ec4ea6cb60f16495ff3687be89ff6`, not the microphone branch.

Its only rendering change replaces endFrame's `glFlush()` with `glFinish()`.
A one-time `ANLAND_GPU_FINISH_PROBE active` log is emitted after the first wait
returns. Original buffer selection, damage tracking, native-fence export and
handoff remain unchanged. This tests whether completing KWin's GL work before
handoff changes the whole-frame backstep symptom. It does not synchronize other
GL contexts or prove correct Android scanout; lower FPS alone is not a successful
result. The extra wait may reduce throughput or increase latency.

There is no APK, microphone, camera, SELinux, Xwayland, kernel or game-setting
change. Do not use the inherited APK artifacts from this branch; only the verified
complete KWin package is relevant. Keep the exact original rollback package and
restore KWin 6.8.0-1.1 after the comparison. The probe does not automatically turn
itself off: stopping the test means reinstalling the stock package and restarting
the compositor normally.

Run the source-order guard with:

```sh
python3 tests/test_gpu_finish_probe.py -v
```

That test is static verification, not a GPU or visual regression test. Require the
native full package build, device dependency checks, the runtime marker and user
observation of the same game scene before interpreting the result.
