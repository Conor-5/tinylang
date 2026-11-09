# tinylang

[![CI](https://github.com/Conor-5/tinylang/actions/workflows/ci.yml/badge.svg)](https://github.com/Conor-5/tinylang/actions/workflows/ci.yml)

A small language with Python-style syntax for microcontrollers. Programs
are compiled to bytecode and run on a small stack-based VM. Each call to
the VM runs a limited number of instructions, so a task that never stops
cannot hold up the others.

The core is portable C99 with no dynamic memory and no platform
dependencies. A host (Linux today, ESP32 planned) embeds it through two
small headers.

Final year BSc project. Correctness, clarity and predictable behaviour
come before features.

```python
x = 10
while x > 0:
    if x % 3 == 0:
        print(x)
    elif x == 5:
        print(-1)
    else:
        pass
    x = x - 1
```

```
9
6
-1
3
```

## Status

Spec 0001 (core language on Linux) is done:

- 32-bit wrapping integers, `True`/`False`, variables
- `if`/`elif`/`else`, `while`, `pass`, `print`
- Operators `+ - * // %`, comparisons, `and`/`or`/`not`
- Instruction budget per VM call
- Linux host with file mode and an interactive REPL

Next: testing tools (spec 0002), then tasks and the ESP32 port.

## Quick start

Needs a C99 compiler and `make` (developed with GCC 11 on Ubuntu 22.04).

```sh
make -C tests                      # build everything and run all tests
./build/normal/tl program.py       # run a file
./build/normal/tl                  # start the REPL
```

`make -C tests` builds twice, once normally and once with
`-fsanitize=address,undefined`, both with `-Werror`. Use
`make -C tests normal` for just the normal build.

```
>>> x = 5
>>> while x > 3:
...     print(x)
...     x = x - 1
...
5
4
>>> x
3
```

## Documentation

| Document                                                 | Contents                                    |
| -------------------------------------------------------- | ------------------------------------------- |
| [`docs/language-reference.md`](docs/language-reference.md) | The language as it is now, including errors and limits |
| [`docs/bytecode.md`](docs/bytecode.md)                   | Opcodes, encoding, budget rules              |
| [`docs/decisions/`](docs/decisions/)                     | Why things are the way they are             |
| [`specs/`](specs/)                                       | One spec per change                         |
| [`AGENTS.md`](AGENTS.md)                                 | Rules for working in this repository        |

## Differences from Python

The syntax is a subset of Python, with some deliberate differences:
integers are 32-bit and wrap, `True` prints as `1`, there is no `/` (use
`//`), and variables start at 0. See
[Differences from Python](docs/language-reference.md#differences-from-python).

## Embedding

A host implements `include/lang_port.h` (two output functions) and calls
`include/lang.h`:

```c
static lang_names_t names;     /* zeroed: no variables yet */
static lang_program_t prog;
static lang_vm_t vm;
char err[80];

if (!lang_compile(src, &names, false, &prog, err, sizeof err)) {
    /* err holds "line N: message" */
}
lang_vm_init(&vm, &prog);
while (lang_vm_run(&vm, 1000) == LANG_BUDGET) {
    /* let other tasks run */
}
```

`ports/linux/main.c` is a complete example.

## Repository layout

| Path              | Contents                                        |
| ----------------- | ----------------------------------------------- |
| `include/`        | Public API: `lang.h`, `lang_port.h`             |
| `src/`            | Tokenizer, compiler, VM                         |
| `ports/linux/`    | Linux host: file runner and REPL                |
| `tests/programs/` | Golden tests: `name.py` + `name.expected`       |
| `tests/repl/`     | REPL tests: `name.in` + `name.expected`         |
| `tests/unit/`     | Unit tests in C                                 |
| `docs/`           | Language and bytecode references, decisions     |
| `specs/`          | Specs, numbered                                 |
| `tools/`          | Test tools (planned)                            |
