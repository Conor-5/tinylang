# 0004: Stack limits are a compile-time error

**Status:** Accepted (spec 0001)

## Context

The VM stack has 16 cells. An expression like `1+(1+(1+...))` needs one
cell per level. Either the VM finds out at run time, or the compiler
finds out first.

## Decision

While emitting code, the compiler tracks how many stack cells the code
needs at each point (every opcode has a known effect). If it would exceed
`LANG_STACK_MAX`, compiling fails with `expression too complex`, with the
line number.

## Why

- A program that compiles cannot fail with a stack error, ever. On a
  device, finding a problem when compiling is much better than a task
  stopping hours later on a rare path
- The check is nearly free: one counter in the compiler
- `and`/`or` jumps leave the same depth on both paths, so a simple
  counter is exact

## Consequences

- `stack overflow` and `stack underflow` remain in the VM as a safety net
  for corrupt or hand-written bytecode, and are tested that way
- Nesting of brackets and unary operators has its own limit (32,
  `too deeply nested`), which bounds the compiler's recursion. A deeply
  bracketed expression like `((((1))))` uses little stack, so the two
  limits are separate
