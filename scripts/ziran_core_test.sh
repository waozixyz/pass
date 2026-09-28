#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$repo/build/ziran-core-test

"$ziran" fmt --check "$repo/pass_core.zi" "$repo/pass_core_test.zi"
"$ziran" check --project --locked "$repo/pass_core_test.zi"
"$ziran" bundle --module-path "$repo" --root "$repo" \
    --entry pass_core_test:PortableAnswer -o "$work.zib" \
    "$repo/pass_core_test.zi"
test "$("$ziran" run "$work.zib")" = 42

rm -rf "$work"
"$ziran" build --project --locked --target=c -o "$work" \
    "$repo/pass_core_test.zi"
cat > "$work/main.c" <<'EOF'
#include "pass_core_test.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
ziran_root=$("$ziran" pkg path ziran --locked)
${CC:-cc} -std=c11 -I"$ziran_root/include" -I"$work" \
    "$work/pass_core.c" "$work/pass_core_test.c" "$work/main.c" \
    -o "$work/runner"
"$work/runner"
