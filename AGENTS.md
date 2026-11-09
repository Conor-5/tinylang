# AGENTS.md

Rules for working in this repository.
Ensure you read this file fully.

## What this project is

A small language with Python-style syntax for microcontrollers. Source code
is compiled to bytecode and run on a small stack-based VM. Each task runs for
a limited number of instructions per step, so a task that never stops cannot
delay the others.

The language core is portable C99. It knows nothing about any RTOS, scheduler
or board. A host (for example the ESP32 firmware in a separate repository)
embeds it by implementing `include/lang_port.h` and calling the API in
`include/lang.h`.

This is a final year BSc project with a long-term aim of industrial use.
Correctness, clarity and predictable behaviour matter more than features.

## Repository layout

| Path                           | Contents                                                             |
| ------------------------------ | -------------------------------------------------------------------- |
| `specs/`                     | One spec per change, numbered (`0001-...md`). Copy`_template.md` |
| `docs/language-reference.md` | The language as it is **now** . Source of truth                |
| `docs/bytecode.md`           | Opcodes, stack effects, bytecode format version                      |
| `docs/decisions/`            | One short decision record per design choice                          |
| `include/`                   | Public API:`lang.h`,`lang_port.h`                                |
| `src/`                       | Tokenizer, compiler, VM                                              |
| `tests/programs/`            | Golden tests:`name.py`+`name.expected`                           |
| `tests/unit/`                | Unit tests in C                                                      |
| `tools/`                     | Test tools (CPython differential testing, fuzzing)                   |
| `ports/linux/`               | REPL and test host for Linux                                         |

## Commands

sh

```sh
make -C tests          # build & run all tests
```

Run the tests after every change and show the output. Never report a task as
done without a passing test run in the same session.

## Hard rules for `src/` and `include/`

* C99 only. Build cleanly with `-std=c99 -Wall -Wextra -Wpedantic -Werror`
* No platform headers: no ESP-IDF, FreeRTOS, POSIX or OS calls. Only the C
  standard library headers needed for freestanding-style code
  (`stdint.h`, `stdbool.h`, `stddef.h`, `string.h`)
* No `malloc` or `free`. All memory is static or provided by the host,
  with sizes set by constants in one place
* No undefined behaviour:
  * Integer arithmetic is 32-bit wraparound: compute in `uint32_t`, then convert
  * Division or modulo by zero is a runtime error. `INT32_MIN / -1` wraps
    to `INT32_MIN` and `INT32_MIN % -1` is `0`: handle both explicitly so
    the C `/` and `%` operators never see them (that would be UB)
  * Read multi-byte values from bytecode with `memcpy`, never pointer casts
  * Check every array index and stack access
* Errors never crash. The compiler reports `line N: message` and stops: the
  VM stops the failing task with a message. No `abort`, `assert` or `exit`
* All output goes through `lang_port.h`, never `printf` directly

## Working from specs

1. Work only from a spec in `specs/`. If something is unclear or the spec
   seems wrong, stop and ask. Do not guess
2. Write or update tests first, from the spec's examples and "Done when" list
3. Implement in small steps:run `make -C tests` after each one
4. When the spec is done, update `docs/language-reference.md` and
   `docs/bytecode.md` so they describe the new behaviour, and add a decision
   record to `docs/decisions/` for any design choice the spec left open
5. Tick the spec's "Done when" boxes only for items you have verified, and
   set its status to `Done`

Do not change a spec's goal or scope yourself. Suggest changes instead.

## Tests

* Never weaken, delete or edit a test just to make it pass. If you believe a
  test is wrong, say so and explain why
* Every example in `docs/language-reference.md` must also exist as a golden
  test in `tests/programs/`
* Every error message the language can produce needs at least one test
* Tests must be deterministic: no real time, no randomness without a fixed seed

## Code style

* Small files and small functions. One job per file
* Comment the *why* of non-obvious code: indentation handling, jump
  patching, the budget check, anything a reader would ask about
* Each file starts with a header comment saying what it does
* Names: `lang_` prefix for public functions and types; `static` for
  everything else

## Do not

* Add features, syntax or built-ins that no spec asks for
* Add dependencies or third-party code
* Change the bytecode format without bumping its version in `docs/bytecode.md`
* Touch `.github/` or this file unless the spec says so
* Flash hardware or run anything outside this repository

## Finishing a task

End with a short summary: what changed, the test output, anything left
undone, and any decision you made that the spec did not cover.
