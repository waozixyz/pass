#!/usr/bin/env bash
set -euo pipefail

root_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
site_dir="$root_dir/build/site"
port="${PASS_WEB_SMOKE_PORT:-18080}"
url="http://127.0.0.1:$port/app/"
tmp_dir=$(mktemp -d)
server_pid=""

cleanup() {
  if [ -n "$server_pid" ] && kill -0 "$server_pid" 2>/dev/null; then
    kill "$server_pid" 2>/dev/null || true
    wait "$server_pid" 2>/dev/null || true
  fi
  rm -rf "$tmp_dir"
}
trap cleanup EXIT

for file in "$site_dir/app/index.html" "$site_dir/app/app.js" "$site_dir/app/index.js" "$site_dir/app/index.wasm"; do
  if [ ! -s "$file" ]; then
    echo "missing web build artifact: $file" >&2
    exit 1
  fi
done

browser="${PASS_WEB_SMOKE_BROWSER:-}"
if [ -z "$browser" ]; then
  for candidate in chromium chromium-browser google-chrome chrome; do
    if command -v "$candidate" >/dev/null 2>&1; then
      browser="$candidate"
      break
    fi
  done
fi
if [ -z "$browser" ]; then
  echo "web smoke: skipped, no Chromium-compatible browser found" >&2
  exit 0
fi

(cd "$site_dir" && python3 -m http.server "$port" --bind 127.0.0.1 >"$tmp_dir/server.log" 2>&1) &
server_pid=$!

python3 - "$url" <<'PY'
import sys
import time
import urllib.request

url = sys.argv[1]
deadline = time.time() + 10
last_error = None
while time.time() < deadline:
    try:
        with urllib.request.urlopen(url, timeout=1) as response:
            if response.status == 200:
                sys.exit(0)
    except Exception as exc:
        last_error = exc
    time.sleep(0.2)
raise SystemExit(f"web smoke: server did not answer {url}: {last_error}")
PY

screenshot="$tmp_dir/pass-web.png"
dom="$tmp_dir/dom.html"
"$browser" \
  --headless=new \
  --no-sandbox \
  --disable-gpu \
  --disable-dev-shm-usage \
  --user-data-dir="$tmp_dir/chrome" \
  --window-size=720,740 \
  --virtual-time-budget=15000 \
  --screenshot="$screenshot" \
  --dump-dom \
  "$url" >"$dom"

if [ ! -s "$screenshot" ]; then
  echo "web smoke: browser did not write a screenshot" >&2
  exit 1
fi
if ! grep -q '<canvas' "$dom"; then
  echo "web smoke: app DOM did not include the canvas" >&2
  exit 1
fi

python3 - "$screenshot" <<'PY'
import struct
import sys
import zlib

path = sys.argv[1]
with open(path, "rb") as handle:
    data = handle.read()
if not data.startswith(b"\x89PNG\r\n\x1a\n"):
    raise SystemExit("web smoke: screenshot is not a PNG")

pos = 8
width = height = bit_depth = color_type = None
compressed = bytearray()
while pos < len(data):
    size = struct.unpack(">I", data[pos:pos + 4])[0]
    kind = data[pos + 4:pos + 8]
    payload = data[pos + 8:pos + 8 + size]
    pos += 12 + size
    if kind == b"IHDR":
        width, height, bit_depth, color_type = struct.unpack(">IIBB", payload[:10])[:4]
    elif kind == b"IDAT":
        compressed.extend(payload)
    elif kind == b"IEND":
        break

if width is None or height is None:
    raise SystemExit("web smoke: PNG has no IHDR")
if bit_depth != 8 or color_type not in (2, 6):
    raise SystemExit(f"web smoke: unsupported PNG format bit_depth={bit_depth} color_type={color_type}")

channels = 4 if color_type == 6 else 3
stride = width * channels
raw = zlib.decompress(compressed)
rows = []
offset = 0
previous = bytearray(stride)
for _ in range(height):
    filter_type = raw[offset]
    offset += 1
    current = bytearray(raw[offset:offset + stride])
    offset += stride
    for index in range(stride):
        left = current[index - channels] if index >= channels else 0
        up = previous[index]
        upper_left = previous[index - channels] if index >= channels else 0
        if filter_type == 1:
            current[index] = (current[index] + left) & 0xff
        elif filter_type == 2:
            current[index] = (current[index] + up) & 0xff
        elif filter_type == 3:
            current[index] = (current[index] + ((left + up) // 2)) & 0xff
        elif filter_type == 4:
            predictor = left + up - upper_left
            pa = abs(predictor - left)
            pb = abs(predictor - up)
            pc = abs(predictor - upper_left)
            current[index] = (current[index] + (left if pa <= pb and pa <= pc else up if pb <= pc else upper_left)) & 0xff
        elif filter_type != 0:
            raise SystemExit(f"web smoke: unsupported PNG filter {filter_type}")
    rows.append(current)
    previous = current

colors = set()
step_y = max(1, height // 80)
step_x = max(1, width // 80)
for y in range(0, height, step_y):
    row = rows[y]
    for x in range(0, width, step_x):
        start = x * channels
        colors.add(tuple(row[start:start + channels]))
        if len(colors) >= 16:
            print(f"web smoke: rendered {width}x{height} canvas screenshot with varied pixels")
            raise SystemExit(0)
raise SystemExit(f"web smoke: screenshot looked blank, only {len(colors)} sampled colors")
PY
