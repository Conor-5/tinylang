# 0002: `//` floors, and there is no `/`

**Status:** Accepted (spec 0001)

## Context

There are only integers. Integer division has to round somehow, and the
syntax should not surprise someone who knows Python.

## Decision

`//` is floor division, rounding towards minus infinity, exactly like
Python's `//`: `-7 // 2` is `-4`. `%` is the matching modulo, with the
sign of the right operand: `-7 % 2` is `1`. `/` is not an operator; using
it is the compile error `'/' is not supported, use '//'`.

## Why

- In Python, `/` is true division (`7 / 2` is `3.5`). Making `/` mean
  integer division would silently give different answers for code that
  looks like Python. An error with a hint is better than a wrong number
- Floor division keeps `(a // b) * b + a % b == a` with `a % b` having the
  sign of `b`, the same as Python, so code and intuition carry over
- Code that compiles here means the same in CPython, which makes the
  planned CPython differential testing straightforward

## Consequences

- C's `/` and `%` truncate towards zero, so the VM adjusts the result
  when the signs differ
- `INT32_MIN // -1` would overflow; it wraps to `INT32_MIN`, and
  `INT32_MIN % -1` is `0`. Both are special-cased so C's operators never
  see them (that would be undefined behaviour)
- If floats are added later, `/` can be added as true division without
  breaking any existing program
