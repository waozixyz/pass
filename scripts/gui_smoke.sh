#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
work="$root/build/gui-smoke"
mkdir -p "$work"
mkdir -p "$work/light" "$work/dark"
# Read only disposable fixture settings, never the user's saved app data.
printf '1\n' > "$work/light/.kryon_pass_theme_mode.txt"
printf '2\n' > "$work/dark/.kryon_pass_theme_mode.txt"
cd "$work/light"
env -u DISPLAY -u WAYLAND_DISPLAY xvfb-run -a -e /dev/stderr \
    env KRYON_CAPTURE_PATH="$work/pass.png" "$root/build/pass-gui"
test -s "$work/pass.png"
cd "$work/dark"
env -u DISPLAY -u WAYLAND_DISPLAY xvfb-run -a -e /dev/stderr \
    env KRYON_CAPTURE_PATH="$work/pass-dark.png" "$root/build/pass-gui"
test -s "$work/pass-dark.png"
echo 'Ziran desktop: light and dark frames rendered on private Xvfb displays'
