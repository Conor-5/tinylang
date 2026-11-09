# 0001: A stack VM

**Status:** Accepted (spec 0001)

## Context

Source code is compiled to bytecode that runs on a microcontroller, a
limited number of instructions at a time, so a task can be paused between
any two instructions. The VM must be small, easy to check for undefined
behaviour, and easy for a single-pass compiler to target.

## Decision

A stack machine: instructions take their operands from a small data stack
(16 cells) and push their result. Variables live in a fixed array of 16
slots. Opcodes are one byte with inline operands.

## Why

- A single-pass recursive descent compiler maps onto a stack directly:
  compile the operands, then emit the operator. No register allocation
- Code is compact (most instructions are one byte), which matters with
  1024 bytes per program
- All VM state is a handful of fields (`pc`, `sp`, stack, variables), so
  pausing and resuming a task is just returning from `lang_vm_run`
- Every stack access is one bounds check, so safety is easy to verify

## Consequences

- Slower than a register VM: more instructions per expression. Accepted:
  predictability matters more than speed here
- The stack size limits expression depth. The compiler checks this
  (decision 0004)
