#!/bin/sh
set -eu

ziran=${1:?pass the ziran command}
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$repo/build/ziran-core-test

"$ziran" fmt --check "$repo/pass_core.zi" "$repo/pass_core_test.zi"
"$ziran" check --root "$repo" "$repo/pass_core_test.zi"
"$ziran" bundle --root "$repo" --entry pass_core_test:PortableAnswer \
    -o "$work.zib" "$repo/pass_core_test.zi"
test "$("$ziran" run "$work.zib")" = 42

rm -rf "$work"
"$ziran" build --target=c --root "$repo" -o "$work" \
    "$repo/pass_core_test.zi"
cat > "$work/main.c" <<'EOF'
#include "pass_core_test.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
${CC:-cc} -std=c11 -I"$(dirname "$ziran")/../../include" -I"$work" \
    "$work/pass_core.c" "$work/pass_core_test.c" "$work/main.c" \
    -o "$work/runner"
"$work/runner"
