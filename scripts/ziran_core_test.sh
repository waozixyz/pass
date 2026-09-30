#!/bin/sh
set -eu
ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
ziran_root=$("$ziran" pkg path ziran)
mkdir -p build/core-test
"$ziran" check --root tests --module-path . tests/core_main.zi
"$ziran" build --target=c --root tests --module-path . -o build/core-test/c tests/core_main.zi
${CC:-cc} -std=c99 -O2 -I"$ziran_root/include" -Ibuild/core-test/c build/core-test/c/*.c -o build/core-test/native
build/core-test/native
"$ziran" bundle --module-path . --root . --entry pass_core_test:PortableAnswer -o build/core-test/core.zib pass_core_test.zi
test "$("$ziran" run build/core-test/core.zib)" = 42
"$ziran" ir --root . -o build/core-test/ir pass_core_test.zi
"$ziran" bundle --root build/core-test/ir --entry pass_core_test:PortableAnswer -o build/core-test/saved.zib build/core-test/ir/pass_core_test.zir
test "$("$ziran" run build/core-test/saved.zib)" = 42
echo 'Ziran core: native, source bundle, and saved IR tests pass'
