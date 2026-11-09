#!/bin/sh
# run_golden.sh - runs every DIR/*.py with the tl binary and compares its
# output (stdout and stderr together) with DIR/*.expected.
#
# Also checks the exit code: 1 if the expected output contains a compile
# error ("line N: ...") or a runtime error, otherwise 0.
#
# Usage: run_golden.sh path/to/tl DIR

tl=$1
dir=$2
pass=0
fail=0
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

for src in "$dir"/*.py; do
    name=${src%.py}
    expected=$name.expected
    if [ ! -f "$expected" ]; then
        echo "  FAIL $src: no $expected"
        fail=$((fail + 1))
        continue
    fi
    "$tl" "$src" >"$tmp" 2>&1
    code=$?
    want_code=0
    if grep -Eq '^(line [0-9]+: |runtime error: )' "$expected"; then
        want_code=1
    fi
    if ! cmp -s "$tmp" "$expected"; then
        echo "  FAIL $src: output differs"
        diff "$expected" "$tmp" | head -n 10 | sed 's/^/    /'
        fail=$((fail + 1))
    elif [ "$code" -ne "$want_code" ]; then
        echo "  FAIL $src: exit code $code, want $want_code"
        fail=$((fail + 1))
    else
        pass=$((pass + 1))
    fi
done

echo "golden         $pass passed, $fail failed"
[ "$fail" -eq 0 ]
