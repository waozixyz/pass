#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
exec env -u DISPLAY -u WAYLAND_DISPLAY xvfb-run -a -e /dev/stderr \
    env PASS_TEST_PRIVATE_DISPLAY=1 python3 scripts/gui_input_test.py
