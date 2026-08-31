#!/bin/sh
set -eu

cli=${1:-./pass}
tmp=${TMPDIR:-/tmp}/pass-cli-test.$$

cleanup()
{
    rm -rf "$tmp"
}
trap cleanup EXIT INT TERM

mkdir -p "$tmp"

fail()
{
    echo "FAIL cli: $*" >&2
    exit 1
}

expect_out()
{
    name=$1
    want=$2
    shift 2
    got=$("$@")
    test "$got" = "$want" || fail "$name: got '$got', want '$want'"
    echo "ok   cli: $name"
}

expect_fail()
{
    name=$1
    status=$2
    needle=$3
    shift 3
    set +e
    "$@" >"$tmp/out" 2>"$tmp/err"
    got_status=$?
    set -e
    test "$got_status" -eq "$status" || fail "$name: status $got_status, want $status"
    grep -F -- "$needle" "$tmp/err" >/dev/null || fail "$name: missing '$needle'"
    echo "ok   cli: $name"
}

"$cli" --version >"$tmp/version"
grep -F -- "pass " "$tmp/version" >/dev/null || fail "version text missing prefix"
echo "ok   cli: version"
expect_out "argument master" "\\g-A1-.OHEwrXjT#" "$cli" lesspass.com contact@lesspass.com password
expect_out "explicit classes" "\\g-A1-.OHEwrXjT#" "$cli" -l -u -d -s lesspass.com contact@lesspass.com password
expect_out "environment master" "\\g-A1-.OHEwrXjT#" env LESSPASS_MASTER_PASSWORD=password "$cli" lesspass.com contact@lesspass.com
expect_out "stdin master" "\\g-A1-.OHEwrXjT#" sh -c "printf 'password\r\n' | \"\$1\" --prompt lesspass.com contact@lesspass.com" sh "$cli"
expect_out "copy disabled" "\\g-A1-.OHEwrXjT#" "$cli" --copy --no-copy lesspass.com contact@lesspass.com password

counter_zero=$("$cli" --counter 0 site login master)
counter_negative=$("$cli" --counter -3 site login master)
test "$counter_negative" = "$counter_zero" || fail "negative counter did not clamp to zero"
echo "ok   cli: negative counter clamp"

"$cli" --help >"$tmp/help"
grep -F -- "Usage: pass" "$tmp/help" >/dev/null || fail "help text missing usage"
echo "ok   cli: help"

expect_fail "unknown option" 2 "unknown option" "$cli" --bogus
expect_fail "missing option value" 2 "requires a value" "$cli" --length
expect_fail "missing exclude value" 2 "requires a value" "$cli" --exclude
expect_fail "invalid option value" 2 "Usage: pass" "$cli" --length nope site login master
expect_fail "missing positional args" 2 "Usage: pass" "$cli" site
expect_fail "too many positional args" 2 "too many positional arguments" "$cli" a b c d
expect_fail "copy unavailable" 2 "--copy is not available" "$cli" --copy site login master
expect_fail "missing stdin master" 2 "missing master password" sh -c "\"\$1\" site login </dev/null" sh "$cli"
expect_fail "core validation" 1 "at least one character class" "$cli" --no-lowercase --no-uppercase --no-digits --no-symbols site login master

echo "all cli tests pass"
