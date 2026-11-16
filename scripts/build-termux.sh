#!/usr/bin/env bash
set -euo pipefail
ndk="${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to an Android NDK installation}"
host="linux-x86_64"
if [[ "$(uname -s)" == Darwin ]]; then host="darwin-x86_64"; fi
toolchain="$ndk/toolchains/llvm/prebuilt/$host/bin"
mkdir -p dist
for target in \
  arm64:aarch64-linux-android31-clang \
  arm:armv7a-linux-androideabi31-clang \
  i686:i686-linux-android31-clang \
  x86_64:x86_64-linux-android31-clang; do
  arch="${target%%:*}"
  cc="$toolchain/${target#*:}"
  mkdir -p "dist/$arch"
  "$cc" -D_GNU_SOURCE -Ifauxdev/include -Ifauxdev/src -std=c11 -fPIC \
    -O2 -Wall -Wextra -Werror -shared \
    fauxdev/src/fauxdev.c fauxdev/src/probe_backend.c \
    fauxdev/src/video_backend.c fauxdev/src/transport.c \
    -o "dist/$arch/libfauxdev-termux.so" -pthread
  install -m 755 scripts/camera2termux-run "dist/$arch/camera2termux-run"
done
