#!/usr/bin/env bash
set -euo pipefail

root="${1:-.}"
cd "$root"

if [ -f .gitmodules ]; then
    echo "::error file=.gitmodules::Dependencies are Ziran packages in ziran.toml, not submodules" >&2
    exit 1
fi

if [ -d vendor ]; then
    echo "::error file=vendor::Legacy vendored dependencies must be declared in ziran.toml" >&2
    exit 1
fi

bad=$(grep -nE '^[[:space:]]*git[[:space:]]*=' ziran.toml \
    | grep -vE '^[0-9]+:[[:space:]]*git[[:space:]]*=[[:space:]]*"https://' || true)
if [ -n "$bad" ]; then
    echo "::error file=ziran.toml::Non-HTTPS package URL(s) break fresh clones and CI:$bad" >&2
    exit 1
fi

echo "all package URLs HTTPS - ok"
