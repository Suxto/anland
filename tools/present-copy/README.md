# Present COPY isolation probe

**Rejected in live DS3 testing:** Xwayland returned actual COPY completions, but
severe visible twitching remained. The candidate was rolled back. Retained only
as diagnostic evidence, not a recommended workaround; do not automatically retry.

A private, per-application Linux preload diagnostic. It ORs the standard
`XCB_PRESENT_OPTION_COPY` bit into Mesa's `xcb_present_pixmap` and (with Present
1.4 headers) `xcb_present_pixmap_synced` requests. All other arguments, fences,
timeline points and returned cookies are forwarded unchanged. It does not wait
for the GPU, invent an idle event, or change the driver's synchronization.

Unlike the software `xcb_put_image` / MIT-SHM workaround, this leaves the
application's GPU rendering and DMA-BUF import path intact. It asks Xwayland to
copy rather than directly flip the client pixmap; whether that copy is GPU
accelerated depends on Xwayland's active rendering backend. This is an isolation
experiment, **not a confirmed correction for frame regression**.

At most 16 initial requests and 16 initial matching Present completions are
logged to stderr per loaded instance. The two special-event wrappers return the
original event pointer and bytes without consuming, modifying or freeing it.
Actual COPY completions are evidence of routing, not proof of physical display
correctness.

```sh
bash tools/present-copy/build-test.sh /tmp/present-copy-build
SANITIZE=address,undefined bash tools/present-copy/build-test.sh /tmp/present-copy-asan
```

The separate mock DSO exercises real `LD_PRELOAD` / `RTLD_NEXT` resolution,
every combination of the low option bits, 64-bit timing values and timeline
points, unchanged event ownership/content, null/foreign/short-event handling,
and bounded logging. These tests need no X server or GPU. Older headers test the
legacy API; Present 1.4 headers also test the synced API.

Only load into the intended application, with a verified compatible DSO for
each architecture used by its runtime. Do not install as a global preload,
replace system libxcb, weaken security policy, or combine it with unrelated
latency / driver changes. Keep an exact known-working launcher backup. Runtime
and visual validation, including rollback on uncertain results, are still
required.
