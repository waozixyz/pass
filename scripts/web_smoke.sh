#!/usr/bin/env bash
set -euo pipefail
root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root_dir"
exec env -u DISPLAY -u WAYLAND_DISPLAY node scripts/web_test.mjs
