#!/bin/sh
# Run the Ziran toolchain commit pinned by ziran.lock. Package commands fetch
# their own locked sources into the user's Ziran cache.
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

toolchain_field() {
    python3 -c 'import json, sys; print(json.load(open("ziran.lock"))["toolchain"][sys.argv[1]])' "$1"
}

tool_url=$(toolchain_field url)
tool_commit=$(toolchain_field commit)
bootstrap="$root/build/ziran-bootstrap"
ziran="$bootstrap/build/bin/ziran"
cache_root="${XDG_CACHE_HOME:-$HOME/.cache}/ziran/sources"
tool_cache="$cache_root/$(python3 -c 'import hashlib, sys; print("p" + hashlib.sha256(f"{sys.argv[1]}\n{sys.argv[2]}".encode()).hexdigest()[:16])' "$tool_url" "$tool_commit")"

current=$(git -C "$bootstrap" rev-parse HEAD 2>/dev/null || true)
if [ "$current" != "$tool_commit" ]; then
    rm -rf "$bootstrap"
    if [ -d "$tool_cache" ]; then
        git clone -q --no-checkout "$tool_cache" "$bootstrap"
        git -C "$bootstrap" checkout -q --detach "$tool_commit"
    else
        mkdir -p "$bootstrap"
        git -C "$bootstrap" init -q
        git -C "$bootstrap" fetch -q --depth 1 "$tool_url" "$tool_commit"
        git -C "$bootstrap" checkout -q --detach FETCH_HEAD
    fi
fi

if [ ! -x "$ziran" ]; then
    env -u DISPLAY -u WAYLAND_DISPLAY make -C "$bootstrap" -s all >&2
fi

exec "$ziran" "$@"
