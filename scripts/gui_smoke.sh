#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
work="$root/build/gui-smoke"
mkdir -p "$work"
env -u DISPLAY -u WAYLAND_DISPLAY xvfb-run -a -e /dev/stderr \
    env KRYON_CAPTURE_PATH="$work/pass.png" ./build/pass-gui
test -s "$work/pass.png"
echo 'Ziran desktop: frame rendered on private Xvfb display'
