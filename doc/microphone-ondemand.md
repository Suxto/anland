# Legacy microphone on demand — candidate

Based on upstream `legacy` commit `a9c8325c190ec4ea6cb60f16495ff3687be89ff6`.
This fork fixes microphone occupancy independently of the kernel and camera work.
It is not installed on the tablet and no real recording test has been performed.

## Behavior

The existing microphone preference remains a permission/consent gate, not an
instruction to capture continuously. Android opens/starts its AAudio input only
when that gate is enabled **and** its connected producer requests capture.

The persistent PipeWire `anland-mic` node advertises a source even while idle.
Its STREAMING transition sends `AUDIO_MSG_CAPTURE_ACTIVE` (type 5), with an exact
native uint32 payload: 1 requests/renews capture; 0 releases it. Source process
callbacks renew the request at most every 250 ms while audio is being requested.
Other source states, teardown, core loss and socket detach send STOP and discard
queued microphone audio. Enumerating a device does not request recording.

Android expires a request after 1000 ms without a renewal. STOP loss, socket
backpressure, a stalled PipeWire graph, or producer death therefore cannot keep
an already-open input indefinitely active. Release is checked on the capture
thread's next pass (an input read can wait up to 100 ms, plus scheduler/API delay).
This is not a hard real-time promise or suppression of Android's privacy indicator.

Only the capture thread opens, reads, stops and closes input. No input is opened
at audio initialization. Each operation owns a temporary socket duplicate so
reconnect/fd reuse cannot redirect it to an unrelated socket. Connection epochs
reject stale requests and are reset when the render thread changes/detaches ctx.
Input formats are read after lazy open, validated, and announced before PCM.
The speaker PCM protocol, microphone consent, and Android permission checks stay
intact; restarting/muting the speaker is not used to release the microphone.

PipeWire's node STREAMING state represents its recording consumers collectively;
if one application ends recording while another still consumes the node, capture
continues. Separate Anland windows retain independent audio bridges; stopping
one closes only its own shared AAudio input, not another window/app's input.

## Parallel APK identity

This branch builds `com.anland.consumer.micondemand`, labelled **Anland Mic Test**.
Java/JNI namespaces remain `com.anland.consumer`; the launcher shortcut and
accessibility component use the new application identity instead of targeting
original-app settings. Installation does not replace `com.anland.consumer`.

The test app starts with its own preferences/permissions. Do not copy the original
app's microphone/camera/accessibility settings or grant permissions implicitly.
Droidspaces' existing launcher still targets the original app until separately
configured for a matched desktop backend. Installing the parallel APK alone does
not enable microphone on demand for an old producer.

## Paired-version requirement

- **New APK + new producer:** microphone on demand.
- **New APK + old producer:** microphone stays off because no capture lease is
  sent. Speaker PCM remains compatible; this is deliberately fail-closed.
- **Old APK + new producer:** the old APK ignores the new request message and
  retains its old continuous-capture behavior. Updating only Linux is NOT a fix.

The shared producer code is consumed by KDE, GNOME, Niri and other backends.
Some backends embed it statically: the ARM64 `libdisplay_producer.so` artifact
is a development candidate, **not** a drop-in replacement for an installed KWin.
Rebuild/relink the matching desktop backend with this source before deployment.
Do not downgrade an installed KWin 6.8.2 to the repository's 6.8.0 recipe.

The APK CI retains upstream debug signing behavior. A newly generated signing
certificate may not match the installed app, so an APK artifact is not proof of
an in-place upgrade path. Do not uninstall the existing app, discard its data,
or replace accessibility/input settings merely to install this candidate.

## Verification

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/audio -v
```

The native test compiles the **actual** Android implementation against test-only
AAudio/log mocks. It exercises idle, demand/renewal/STOP, user consent, lease
expiry, invalid/truncated control, same-fd reconnect, independent windows,
playback coexistence and peer disconnect. No microphone is opened by this test.
The producer test compiles the actual PipeWire-side implementation but uses only
private socketpairs/state callbacks; it does not connect a real recording source.

CI gates APK/daemon builds on these tests and builds the real ARM64 producer
library against PipeWire. Real graph transitions with multiple recording clients,
Android input permission denial/device errors, privacy indicator release, audio
quality and matched-desktop integration still require separate device validation.
No pipeline auto-installation or physical audio capture is added.
