# 0007: The compiler keeps its state in static memory

**Status:** Accepted (implementation of spec 0001; the spec did not say)

## Context

The compiler's working state (tokenizer, a copy of the names table, error
text) is about 800 bytes. It has to live somewhere, without `malloc`.

## Decision

`lang_compile` keeps its state in one `static` variable. As a result
`lang_compile` is not re-entrant: only one compile may run at a time. This
is stated in `lang.h`.

## Why

On a microcontroller, task stacks are often only a few KB. Putting 800
bytes on the caller's stack, on top of the parser's recursion, is a
likely stack overflow that would be hard to diagnose. Static memory is
counted at link time.

## Consequences

An RTOS host that compiles from more than one task must serialise the
calls (e.g. with a mutex), or compile from one task only. The VM has no
such restriction: each `lang_vm_t` is independent. If this becomes a
problem, the state can move into a struct the host provides, which is an
API change.
