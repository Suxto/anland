#!/bin/sh
set -eu
# SOURCE is a pristine, pinned Mesa checkout. Build/installation stays private.
SOURCE=$1
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
patch -d "$SOURCE" -p1 --forward < "$HERE/mesa.patch"
# Match the installed libgallium SONAME and its default symbol versions: the
# stock EGL/GLES libraries also use this DSO for shared GLAPI entry points.
printf '%s\n' '26.3.0-arch1.1' > "$SOURCE/VERSION"
for sanitizer in no yes; do
  flags=''
  if [ "$sanitizer" = yes ]; then flags='-fsanitize=address,undefined -fno-omit-frame-pointer'; fi
  cc -std=gnu11 -Wall -Wextra -Werror -g $flags \
    -I"$SOURCE" -I"$SOURCE/src/freedreno/drm" -I"$HERE" "$HERE/test-submit.c" -o "/tmp/kgsl-submit-$sanitizer"
  "/tmp/kgsl-submit-$sanitizer"
done
meson setup "$SOURCE/build-anland" "$SOURCE" --prefix=/usr \
  -Dplatforms=x11,wayland -Dgallium-drivers=freedreno,softpipe \
  -Dfreedreno-kmds=kgsl -Dvulkan-drivers= -Dvulkan-layers= \
  -Dllvm=disabled -Dgallium-va=disabled -Dgallium-mediafoundation=disabled \
  -Dglx=dri -Dglvnd=enabled -Degl=enabled -Dgles1=enabled -Dgles2=enabled \
  -Dlibunwind=disabled -Dvalgrind=disabled -Dbuild-tests=false -Dbuildtype=release
ninja -C "$SOURCE/build-anland" -j4 src/gallium/targets/dri/libgallium-26.3.0-arch1.1.so
mkdir -p "$HERE/artifact/dri"
cp "$SOURCE/build-anland/src/gallium/targets/dri/libgallium-26.3.0-arch1.1.so" "$HERE/artifact/"
ln -s ../libgallium-26.3.0-arch1.1.so "$HERE/artifact/dri/kgsl_dri.so"
meson configure "$SOURCE/build-anland" > "$HERE/artifact/build-options.txt"
readelf -d "$HERE/artifact/libgallium-26.3.0-arch1.1.so" > "$HERE/artifact/dynamic.txt"
sha256sum "$HERE/artifact/libgallium-26.3.0-arch1.1.so" > "$HERE/artifact/sha256.txt"
