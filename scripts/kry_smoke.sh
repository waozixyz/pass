#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
kryon_dir=${KRYON_DIR:-"$root/vendor/kryon"}
arch=$(uname -m)
if [ "$arch" = "amd64" ]; then
    arch=x86_64
fi
platform=$(uname -s | tr '[:upper:]' '[:lower:]')
build_dir="$kryon_dir/build/$platform-$arch"
k2g="$build_dir/bin/k2g"
k2c="$build_dir/bin/k2c"
work=${TMPDIR:-/tmp}/pass-kry-smoke.$$

cleanup() { rm -rf "$work"; }
trap cleanup EXIT INT TERM

make -C "$kryon_dir" k2g k2c

mkdir -p "$work/go" "$work/c"
"$k2g" --root "$root" -o "$work/go" "$root/app/pass.kry"
"$k2c" --root "$root" -o "$work/c" "$root/app/pass.kry"

sh "$kryon_dir/tests/check_clean_generated_output.sh" "$work/go"
sh "$kryon_dir/tests/check_clean_generated_output.sh" "$work/c"

cat > "$work/go/go.mod" <<EOF
module passkrysmoke

go 1.25.0

require (
	github.com/waozixyz/kryon/go/kryon v0.0.0
	golang.org/x/image v0.45.0 // indirect
	golang.org/x/sys v0.47.0 // indirect
	golang.org/x/text v0.41.0 // indirect
)

replace github.com/waozixyz/kryon/go/kryon => $kryon_dir/go/kryon
EOF
cp "$kryon_dir/go/kryon/go.sum" "$work/go/go.sum"

(cd "$work/go" && GOCACHE="${GOCACHE:-$work/go-cache}" go test ./...)
cc -fsyntax-only -I"$kryon_dir/include" -I"$work/c" "$work/c/app/pass.c"

echo "pass .kry smoke ok"
