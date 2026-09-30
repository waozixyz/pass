#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
ziran_root=$("$ziran" pkg path ziran)
args="--root tests --module-path src --module-path $ziran_root/std"
mkdir -p build/mini-test
"$ziran" build $args --target=c -o build/mini-test/c tests/mini_test.zi
${CC:-cc} -std=c99 -O2 -I"$ziran_root/include" -Ibuild/mini-test/c build/mini-test/c/*.c -o build/mini-test/native
env -u DISPLAY -u WAYLAND_DISPLAY build/mini-test/native
"$ziran" bundle $args --entry mini_test:MiniAnswer -o build/mini-test/portable.zib tests/mini_test.zi
test "$("$ziran" run build/mini-test/portable.zib)" = 42
echo 'Ziran mini mode: native and portable tests pass'
