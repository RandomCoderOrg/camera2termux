#!/usr/bin/env bash
set -euo pipefail
ndk="${ANDROID_NDK_HOME:?Set ANDROID_NDK_HOME to an Android NDK installation}"
host="linux-x86_64"
if [[ "$(uname -s)" == Darwin ]]; then host="darwin-x86_64"; fi
cc="$ndk/toolchains/llvm/prebuilt/$host/bin/aarch64-linux-android31-clang"
mkdir -p dist
"$cc" -D_GNU_SOURCE -Ifauxdev/include -Ifauxdev/src -std=c11 -fPIC \
  -O2 -Wall -Wextra -Werror -shared \
  fauxdev/src/fauxdev.c fauxdev/src/probe_backend.c \
  fauxdev/src/video_backend.c fauxdev/src/transport.c \
  -o dist/libfauxdev-termux.so -pthread
cp scripts/camera2termux-run dist/camera2termux-run
chmod +x dist/camera2termux-run
tar -C dist -czf dist/camera2termux-termux-arm64.tar.gz \
  libfauxdev-termux.so camera2termux-run
