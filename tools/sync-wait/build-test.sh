#!/usr/bin/env bash
set -euo pipefail
umask 077
src=$(cd -- "$(dirname -- "$0")" && pwd)
out=${1:?Usage: build-test.sh OUTPUT_DIRECTORY}
mkdir -p -- "$out"
cd -- "$out"
cc=${CC:-cc}
flags=(-std=gnu11 -O2 -g -Wall -Wextra -Werror)
if [[ -n ${SANITIZE:-} ]]; then flags+=(-fsanitize="$SANITIZE" -fno-omit-frame-pointer); fi
# Verify the exact Linux ABI and libdrm signature used by the portable probe.
# shellcheck disable=SC2046
"$cc" "${flags[@]}" $(pkg-config --cflags libdrm) -x c -c -o uapi-check.o - <<'C'
#include <sys/ioctl.h>
#include <linux/dma-buf.h>
#include <stddef.h>
#include <xf86drm.h>
_Static_assert(DMA_BUF_IOCTL_IMPORT_SYNC_FILE == 0x40086203UL, "command mismatch");
_Static_assert(sizeof(struct dma_buf_import_sync_file) == 8, "size mismatch");
_Static_assert(offsetof(struct dma_buf_import_sync_file, fd) == 4, "offset mismatch");
int (*checked_drm_ioctl)(int, unsigned long, void *) = drmIoctl;
C
"$cc" "${flags[@]}" -fPIC -shared "$src/sync-wait.c" -o libanland_sync_wait.so -Wl,--no-as-needed -ldrm -Wl,--as-needed -ldl
"$cc" "${flags[@]}" -fPIC -shared "$src/fake-drm.c" -o libfakedrm.so
"$cc" "${flags[@]}" "$src/test-sync.c" -o test-sync -L. -lfakedrm '-Wl,-rpath,$ORIGIN' -pthread
preload="$PWD/libanland_sync_wait.so"
if [[ ${SANITIZE:-} == *address* ]]; then preload="$("$cc" -print-file-name=libasan.so):$preload"; fi
for mode in observe wait; do
    ANLAND_WSI_SYNC_MODE="$mode" LD_PRELOAD="$preload" ./test-sync "$mode" > "$mode.log" 2> "$mode.err"
done
printf '%s: UAPI, passthrough and bounded producer-wait tests passed\n' "$(uname -m)"
sha256sum libanland_sync_wait.so
