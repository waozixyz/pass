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
k2c="$build_dir/bin/k2c"
work=${TMPDIR:-/tmp}/pass-kry-smoke.$$
kry_files=$(find "$root/app" -type f -name '*.kry' | LC_ALL=C sort)

cleanup()
{
    rm -rf "$work"
}
trap cleanup EXIT INT TERM

make -C "$kryon_dir" k2c

mkdir -p "$work/c"
"$k2c" --no-main --root "$root" -o "$work/c" $kry_files

sh "$kryon_dir/tests/check_clean_generated_output.sh" "$work/c"

c_amalgam="$work/pass_kry_generated.c"
pass_c=
for file in $(find "$work/c" -type f -name '*.c' | LC_ALL=C sort); do
    case "$file" in
        */app/pass.c)
            pass_c=$file
            ;;
        *)
            printf '#include "%s"\n' "$file" >> "$c_amalgam"
            ;;
    esac
done
if [ -n "$pass_c" ]; then
    printf '#include "%s"\n' "$pass_c" >> "$c_amalgam"
fi

cc -fsyntax-only \
    -D_DEFAULT_SOURCE \
    -D_GNU_SOURCE \
    -D_FILE_OFFSET_BITS=64 \
    -I"$kryon_dir/include" \
    -I"$kryon_dir/src" \
    -I"$work/c" \
    -I"$root/native" \
    -I"$root/droid/app/src/main/cpp" \
    "$c_amalgam"

echo "pass .kry to C smoke ok"
