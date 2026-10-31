# Producer sync-file wait probe

DS3-only diagnostic, not a proven presentation fix. No system libraries or
compositor changes. `drmIoctl` is a fixed-signature API, unlike variadic `ioctl`.

After a **successful** Linux `DMA_BUF_IOCTL_IMPORT_SYNC_FILE` carrying WRITE
access, poll the caller's existing producer fence before returning to Mesa WSI.
This includes the rendering/blit completion exported by WSI, not a guess at a
Vulkan queue. It does not consume/close the FD, rewrite a fence, copy image data,
force COPY presentation, alter frame latency, or synchronize compositor release.

- Default / `ANLAND_WSI_SYNC_MODE=observe`: preserve results and errno; bounded
  logging of imports and initially pending fences.
- `ANLAND_WSI_SYNC_MODE=wait`: wait at most 1000 ms using a monotonic deadline,
  adjusting for EINTR. Invalid fences or timeout return an error rather than
  pretending that a frame is ready. Real GPU failure may therefore stop a test.
- Initial import, pending-fence and error logs are separately bounded to eight.
- Never dereference a failed ioctl's payload; successful unaligned payloads are
  read with memcpy. The caller retains all FD/payload ownership.

Build and test on Linux:

```sh
bash tools/sync-wait/build-test.sh /tmp/sync-wait-build
SANITIZE=address,undefined bash tools/sync-wait/build-test.sh /tmp/sync-wait-asan
```

Checks cover the actual Linux UAPI/libdrm signature, passthrough, errno, EFAULT
with an invalid pointer, unaligned payloads, READ-only exclusion, delayed
readiness, no consumption/closure, and timeout. Pipe readiness in a mock DSO is
only an interface test, not evidence that a real Vulkan fence tracks the intended
work. Confirm actual imports/pending waits in the game and judge the visible
result separately. Keep byte-exact rollback to the known working profile.
