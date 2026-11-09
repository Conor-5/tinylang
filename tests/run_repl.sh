#!/bin/sh
# run_repl.sh - feeds each DIR/*.in to the tl REPL on stdin and compares
# everything it prints (prompts included) with DIR/*.expected.
#
# Prompts are printed without a newline, so in the expected files a prompt
# is directly followed by the output of the line typed after it.
#
# Usage: run_repl.sh path/to/tl DIR

tl=$1
dir=$2
pass=0
fail=0
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

for input in "$dir"/*.in; do
    expected=${input%.in}.expected
    "$tl" <"$input" >"$tmp" 2>&1
    if cmp -s "$tmp" "$expected"; then
        pass=$((pass + 1))
    else
        echo "  FAIL $input"
        diff "$expected" "$tmp" | head -n 10 | sed 's/^/    /'
        fail=$((fail + 1))
    fi
done

echo "repl           $pass passed, $fail failed"
[ "$fail" -eq 0 ]
