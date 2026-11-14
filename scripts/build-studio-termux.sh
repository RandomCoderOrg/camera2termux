#!/data/data/com.termux/files/usr/bin/bash
set -euo pipefail

if [[ "$(uname -o)" != Android ]]; then
  echo 'Build Camera2Termux Studio inside native Termux with Qt 6 installed.' >&2
  exit 2
fi

root="$(cd "$(dirname "$0")/.." && pwd)"
prefix="${PREFIX:-/data/data/com.termux/files/usr}"
cmake -S "$root/studio" -B "$root/build/studio-termux" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build "$root/build/studio-termux" --parallel 2
cmake --install "$root/build/studio-termux"

cc -D_GNU_SOURCE -std=c11 -O2 -Wall -Wextra -Werror \
  -I"$root/fauxdev/include" -I"$root/fauxdev/src" \
  "$root/fauxdev/tools/camera_inventory.c" "$root/fauxdev/src/transport.c" \
  -o "$prefix/bin/fauxdev-camera-info" -pthread

echo 'Studio installed. Start the Android camera service and Termux:X11, then run:'
echo '  camera2termux-run camera2termux-studio'
