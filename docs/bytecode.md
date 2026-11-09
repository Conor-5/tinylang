# Bytecode

**Format version: 1**

Change this version whenever the meaning of any byte changes: an opcode
value, an operand, or a rule below. Opcode values are listed in
`src/opcodes.h`; never renumber them.

## Program

A program (`lang_program_t`) is:

| Field   | Meaning                                         |
| ------- | ----------------------------------------------- |
| `code`  | Up to `LANG_CODE_MAX` (1024) bytes of bytecode  |
| `len`   | Bytes of `code` in use                          |
| `nvars` | Variable slots the program uses (at most 16)    |

Execution starts at byte 0. Every program the compiler produces ends with
`HALT`.

## Encoding

- Each instruction is a 1-byte opcode, followed by its operand if it has
  one
- Operands are little-endian. The VM assembles them byte by byte, so it
  does not depend on the host's byte order or alignment
- `i32` and `i16` operands are two's complement
- Jump offsets (`i16`) are relative to the byte **after** the operand:
  `target = (address of opcode) + 3 + offset`
- Opcode `0x00` is deliberately unused, so zeroed memory is invalid

## Opcodes

Stack effects are written `before → after`, top of the stack on the right.

| Value  | Opcode                 | Operand | Stack            | Meaning |
| ------ | ---------------------- | ------- | ---------------- | ------- |
| `0x01` | `PUSH_INT`             | `i32`   | `→ v`            | Push the operand |
| `0x02` | `LOAD`                 | `u8`    | `→ v`            | Push variable `slot` |
| `0x03` | `STORE`                | `u8`    | `v →`            | Pop into variable `slot` |
| `0x04` | `POP`                  | —       | `v →`            | Discard the top value |
| `0x05` | `ADD`                  | —       | `a b → a+b`      | Wrapping add |
| `0x06` | `SUB`                  | —       | `a b → a-b`      | Wrapping subtract |
| `0x07` | `MUL`                  | —       | `a b → a*b`      | Wrapping multiply |
| `0x08` | `FLOORDIV`             | —       | `a b → a//b`     | Floor division; `b == 0` is `division by zero`; `INT32_MIN // -1` is `INT32_MIN` |
| `0x09` | `MOD`                  | —       | `a b → a%b`      | Floor modulo (sign of `b`); `b == 0` is `division by zero`; `x % -1` is 0 |
| `0x0A` | `NEG`                  | —       | `a → -a`         | Wrapping negate (`-INT32_MIN` is `INT32_MIN`) |
| `0x0B` | `NOT`                  | —       | `a → !a`         | 1 if `a == 0`, else 0 |
| `0x0C` | `EQ`                   | —       | `a b → a==b`     | 1 or 0 |
| `0x0D` | `NE`                   | —       | `a b → a!=b`     | 1 or 0 |
| `0x0E` | `LT`                   | —       | `a b → a<b`      | 1 or 0 |
| `0x0F` | `GT`                   | —       | `a b → a>b`      | 1 or 0 |
| `0x10` | `LE`                   | —       | `a b → a<=b`     | 1 or 0 |
| `0x11` | `GE`                   | —       | `a b → a>=b`     | 1 or 0 |
| `0x12` | `JUMP`                 | `i16`   | `→`              | Jump |
| `0x13` | `JUMP_IF_FALSE`        | `i16`   | `v →`            | Pop; jump if `v == 0` |
| `0x14` | `JUMP_IF_FALSE_OR_POP` | `i16`   | `v → v` or `v →` | If `v == 0` jump and keep it, else pop it (`and`) |
| `0x15` | `JUMP_IF_TRUE_OR_POP`  | `i16`   | `v → v` or `v →` | If `v != 0` jump and keep it, else pop it (`or`) |
| `0x16` | `PRINT`                | —       | `v →`            | Pop; print in decimal and a newline via `lang_port_write` |
| `0x17` | `HALT`                 | —       | `→`              | Stop: `LANG_DONE` |

## Checks

The VM checks everything, because bytecode might not come from the
compiler. A failed check stops the program with a runtime error:

| Check                                                   | Error             |
| ------------------------------------------------------- | ----------------- |
| Push with 16 values already on the stack                | `stack overflow`  |
| Pop or operate with too few values                      | `stack underflow` |
| Unknown opcode                                          | `invalid bytecode` |
| Operand runs past `len`                                 | `invalid bytecode` |
| Jump target outside `0 .. len-1` (checked whether or not the jump is taken) | `invalid bytecode` |
| Execution reaches `len` without `HALT`                  | `invalid bytecode` |
| `LOAD`/`STORE` slot `>= nvars`                          | `invalid bytecode` |
| `len > 1024`, `nvars > 16`, or no program               | `invalid bytecode` |

For jumps that pop, the stack is checked before the target.

Code from the compiler never fails any check except `division by zero`:
the compiler counts stack use while it emits code (`expression too
complex` if over 16) and only emits valid slots and jumps.

## Budget

`lang_vm_run(vm, budget)` runs at most `budget` instructions; 0 means no
limit.

- One instruction is one opcode executed, `HALT` included. `vm->steps` is
  the number run by the last call
- If `HALT` runs within the budget, the result is `LANG_DONE`, even if it
  was the last step. If the budget runs out first, the result is
  `LANG_BUDGET`, and the next call carries on from the same place
- After `LANG_DONE` or `LANG_ERROR`, further calls run nothing, return the
  same status and set `steps` to 0. The error message is printed once
- `lang_vm_init` and `lang_vm_load` set the status to `LANG_BUDGET`,
  meaning "ready to run"
- Hosts running several tasks must never pass 0

## How the compiler uses it

| Source                     | Bytecode |
| -------------------------- | -------- |
| `x = e`                    | `e; STORE x` |
| expression statement `e`   | `e; POP` (`e; PRINT` at the REPL top level) |
| `a and b`                  | `a; JUMP_IF_FALSE_OR_POP end; b; end:` |
| `a or b`                   | `a; JUMP_IF_TRUE_OR_POP end; b; end:` |
| `-2147483648`              | `PUSH_INT -2147483648` (the only folded constant) |
| `if a: A elif b: B else: C` | `a; JUMP_IF_FALSE L1; A; JUMP end; L1: b; JUMP_IF_FALSE L2; B; JUMP end; L2: C; end:` |
| `while a: A`               | `top: a; JUMP_IF_FALSE end; A; JUMP top; end:` |
| end of program             | `HALT` |
