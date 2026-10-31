#!/usr/bin/env bash
set -euo pipefail
umask 077
src=$(cd -- "$(dirname -- "$0")" && pwd)
out=${1:?Usage: build-test.sh OUTPUT_DIRECTORY}
mkdir -p -- "$out"
cd -- "$out"
cc=${CC:-cc}
flags=(-std=gnu11 -O2 -g -Wall -Wextra -Werror)
if [[ -n ${SANITIZE:-} ]]; then
    flags+=(-fsanitize="$SANITIZE" -fno-omit-frame-pointer)
fi
"$cc" "${flags[@]}" -fPIC -shared "$src/present-copy.c" -o libanland_present_copy.so -lxcb-present -lxcb -ldl
"$cc" "${flags[@]}" -fPIC -shared "$src/fake-present.c" -o libfakepresent.so
"$cc" "${flags[@]}" "$src/test-present.c" -o test-present -L. -lfakepresent '-Wl,-rpath,$ORIGIN'
./test-present plain > plain.log 2> plain.err
preload="$PWD/libanland_present_copy.so"
if [[ ${SANITIZE:-} == *address* ]]; then
    preload="$("$cc" -print-file-name=libasan.so):$preload"
fi
LD_PRELOAD="$preload" ./test-present copy > copy.log 2> copy.err
test "$(grep -c 'ANLAND_PRESENT_COPY request' copy.err)" = 16
test "$(grep -c 'ANLAND_PRESENT_COPY complete' copy.err)" = 16
test ! -s plain.err
printf '%s: ABI passthrough and bounded diagnostics passed\n' "$(uname -m)"
sha256sum libanland_present_copy.so
