#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"
ziran_root=$("$ziran" pkg path ziran)
args="--root tests --module-path src --module-path . --module-path $ziran_root/std"
mkdir -p build/runtime-test
"$ziran" build $args --target=c -o build/runtime-test/c tests/runtime_test.zi
${CC:-cc} -std=c99 -O2 -I"$ziran_root/include" -Ibuild/runtime-test/c build/runtime-test/c/*.c -o build/runtime-test/native
env -u DISPLAY -u WAYLAND_DISPLAY build/runtime-test/native
"$ziran" bundle $args --entry runtime_test:RuntimeAnswer \
    --bind runtime:ReadStore=runtime_host:ReadStore \
    --bind runtime:WriteStore=runtime_host:WriteStore \
    --bind runtime:ClipboardWrite=runtime_host:ClipboardWrite \
    --bind runtime:ClipboardMatches=runtime_host:ClipboardMatches \
    --bind runtime:Now=runtime_host:Now \
    --bind runtime:SecureAvailable=runtime_host:SecureAvailable \
    --bind runtime:SecureSaved=runtime_host:SecureSaved \
    --bind runtime:SecureRequest=runtime_host:SecureRequest \
    --bind runtime:SecureForget=runtime_host:SecureForget \
    --bind runtime:SecurePoll=runtime_host:SecurePoll \
    -o build/runtime-test/portable.zib tests/runtime_test.zi tests/runtime_host.zi
test "$("$ziran" run build/runtime-test/portable.zib)" = 42
echo 'Ziran runtime: native and portable tests pass'
