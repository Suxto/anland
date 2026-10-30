# Private X11 shared-memory copy experiment

This is **not** a global graphics-library replacement or a proven game fix.
It retains `MESA_VK_WSI_DEBUG=sw` and intercepts only large, packed 32-bpp
`xcb_put_image` calls in a specifically opted-in process. Instead of transmitting
pixels through the X11 socket, it copies them into a private mode-0600 SysV SHM
segment and uses standard MIT-SHM PutImage. A checked server round trip precedes
reuse; small, unsupported or failed operations fall back to the original call.
Segments are marked IPC_RMID immediately after attaching. There is no security
policy, kernel, IPC-namespace or display-server configuration change.

## Why

On the tested tablet, native and FEX-visible Adreno both omit
`VK_EXT_external_memory_host`. Mesa's WSI requires that extension for its own SHM
allocation path, even though Xwayland supports MIT-SHM 1.2. Its working software
fallback therefore sends whole images over the socket. This experiment adds one
CPU copy to ordinary X11 SHM without pretending the Vulkan capability exists.

The original `sw` configuration is the known-good fallback. GPU `buffer`, FIFO
and a compositor `glFinish` experiment did not resolve the reported visual issue.

## Checks

`pixel-test.c` uses only private offscreen pixmaps. It checks exact RGB readback,
small-image fallback, resize, two concurrent callers, fork and reconnect. Run it
without the library, with it, and with `ANLAND_XSHM_DISABLE=1`. `--bench` measures
only image transfer and final readback, **not game FPS**.

On-device native ARM64 and FEX x86-64 checks pass. Two FEX 1920x1080 transfer
microbenchmarks measured approximately 3.36–3.39 ms without the shim and
1.14–1.19 ms with it. These are not controlled whole-game benchmarks.
The tablet's 39-bit user virtual-address space prevents the installed ASan runtime
from initializing, so ASan/UBSan are additionally checked in CI, not represented
as having passed on the tablet.

Build:

```sh
cc -std=gnu11 -Wall -Wextra -Werror -O2 -fPIC -shared xshm-copy.c -o libxshm.so -lxcb -lxcb-shm -ldl -pthread
cc -std=gnu11 -Wall -Wextra -Werror -O2 pixel-test.c -o pixel-test -lxcb -pthread
LD_PRELOAD="$PWD/libxshm.so" ./pixel-test
```

## Limits and rollout

Only little-endian ZPixmap depth 24/32 with 32 bits per pixel and exact packed
sizes between 64 KiB and 64 MiB is accelerated. The cache is globally serialized
within one process and bounded to one image. The checked barrier can stall if the
X server stops responding, just like the original WSI's geometry round trip.
This is a CPU-copy compatibility experiment, not a zero-copy implementation.
Do not install the inherited Android APK artifacts from this branch.

A game trial must be opt-in and isolated to DS3, use architecture-matched private
libraries, verify the activation marker and actual library maps, and preserve a
byte-exact launcher rollback. Never modify system libxcb or global LD_PRELOAD.
