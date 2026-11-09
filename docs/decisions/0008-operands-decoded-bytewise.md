# 0008: Operands are decoded byte by byte, not with memcpy

**Status:** Accepted (implementation of spec 0001). Needs review:
AGENTS.md and spec 0001 say "read with `memcpy`"

## Context

Multi-byte operands (`i32`, `i16`) must be read without pointer casts,
which can be unaligned and break strict aliasing. AGENTS.md says to use
`memcpy`. But `memcpy` into an `int32_t` gives the host's byte order, and
the format says operands are little-endian.

## Decision

The VM builds each operand from its bytes with shifts:
`p[0] | p[1] << 8 | ...`, in `uint32_t`, then converts to signed without
implementation-defined behaviour. The compiler writes bytes the same way.

## Why

This meets the purpose of the rule (no casts, no alignment or aliasing
problems) and also makes the bytecode format the same on every host,
including a big-endian one. `memcpy` would only be correct on
little-endian hosts. The ESP32 and x86 are both little-endian, so today
the results are identical.

## Consequences

If you accept this, AGENTS.md should say "read multi-byte values byte by
byte or with `memcpy`, never with pointer casts".
