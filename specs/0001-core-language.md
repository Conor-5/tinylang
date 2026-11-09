# Spec 0001: Core language on Linux

**Status:** Done
**Depends on:** none

## Goal

A working first version of the language on Linux: type a program, it is
compiled to bytecode and run on the VM. Covers expressions, variables,
`if`/`elif`/`else`, `while` and `print`, plus the instruction budget, so a
`while True` loop returns control instead of running forever. This is the
base every later spec builds on (tasks, `wait`, ESP32 port, test tooling).

Where a choice is open, follow Python, so the language is easy to pick up
for anyone who knows Python. Every deliberate difference from Python is
listed under "Differences from Python" and recorded in `docs/decisions/`.

## Context

Empty repository with the layout described in `AGENTS.md`. Follow all rules
in `AGENTS.md`. `include/lang_port.h` and `include/lang.h` are empty and are
created by this spec.

## Scope

In scope:

- Tokenizer with INDENT/DEDENT, parser/compiler (single pass, emits bytecode
  directly), stack VM
- 32-bit integers, `True`/`False`, variables, all operators below
- Statements: assignment, expression, `if`/`elif`/`else`, `while`, `pass`,
  `print(x)`
- Instruction budget per call to the VM
- Linux host in `ports/linux/`: run a file, or an interactive REPL
- Golden tests and unit tests, run by `make -C tests`

Out of scope (later specs):

- `task`, `wait`, `toggle`, `tasks`, `kill`, `budget()` built-ins
- ESP32 port and scheduler integration
- CPython differential testing and fuzzing
- Line numbers in runtime errors (compile errors do have them)
- Strings, functions, lists, `for`, floats, a separate bool type
- `break`, `continue`, augmented assignment (`+=` etc.), `**`, bitwise
  operators, hex/octal/binary literals, backslash line continuation

## Behaviour

### Values and operators

- Integers are 32-bit signed with wraparound on overflow
  (`2147483647 + 1` gives `-2147483648`)
- `True` is 1, `False` is 0. `print(True)` prints `1`
- A value is true if it is not 0 (`if`, `while`, `and`, `or`, `not`)
- Precedence, lowest to highest (same as Python):
  1. `or`
  2. `and`
  3. `not`
  4. `== != < > <= >=` (no chaining: `1 < 2 < 3` is a compile error)
  5. `+ -`
  6. `* // %`
  7. unary `-`
- `//` is integer division rounding **down**, as in Python: `-7 // 2` is
  `-4`. `%` matches it, as in Python: `-7 % 2` is `1`
- `/` does not exist (true division needs floats). Using it is the compile
  error `'/' is not supported, use '//'`
- Division or modulo by zero is a runtime error. `-2147483648 // -1` wraps
  to `-2147483648`; `-2147483648 % -1` is `0`
- Comparisons and `not` give 1 or 0
- `and` / `or` short-circuit and give the operand value, as in Python:
  `2 and 3` is `3`, `0 and 3` is `0`, `0 or 5` is `5`, `2 or 5` is `2`
- Integer literals: decimal only. `0` is allowed; other literals must not
  start with `0` (`007` is `invalid number`, as in Python). A literal must
  fit in 32 bits, with one exception: unary minus applied directly to the
  literal `2147483648` gives `-2147483648`. Any other use of
  `2147483648` or larger is `number too large`
- A number directly followed by a letter, `_` or `.` is `invalid number`
  (`0x10`, `1e5`, `1.5`, `123abc`)

### Statements and blocks

Indentation follows Python's rules:

- A block is indented by any number of spaces, as long as it is more than
  the line that opens it. Every line in the block uses the same
  indentation. A dedent must return to a level already in use
- Tabs in leading whitespace are an error (`tab in indentation`). Tabs
  elsewhere on a line count as ordinary whitespace
- Blank lines and lines containing only a comment (`# ...`) are ignored,
  whatever their indentation. Comments may also end a line
- Inside open brackets `( )`, newlines and indentation are ignored, so an
  expression can continue over several lines (as in Python)
- An empty block is an error (`expected an indented block`); use `pass`
- As in Python, a block can instead be one simple statement on the same
  line after the `:` (`while True: pass`, `if x: print(x)`). Simple
  statements are assignment, expression, `print` and `pass`. A compound
  statement there is an error (`if a: if b: pass` gives
  `expected expression, got 'if'`), and there is no `;`
- At most 16 nested blocks
- Line endings may be `\n` or `\r\n`. The last line does not need a newline

Names:

- A name is a letter or `_`, then letters, digits or `_`, at most 31
  characters
- All Python keywords are reserved. The supported ones are `True`,
  `False`, `and`, `or`, `not`, `if`, `elif`, `else`, `while`, `pass`. Any
  other (`for`, `def`, `return`, `break`, `None`, ...) gives
  `'<keyword>' is not supported`. `print` is also reserved
- A variable must be assigned before it is read anywhere later in the
  source text, otherwise compile error `unknown name 'x'`. This is a check
  on the source text, not on which lines actually run (see "Differences
  from Python")
- Variables start at 0
- Max 16 variables per program (per session in the REPL)

`print`:

- `print(expr)` is a statement. It prints the value in decimal followed by
  a newline
- It takes exactly one argument (`print()` and `print(1, 2)` are
  `print takes exactly 1 argument`)
- It cannot be used inside an expression (`y = print(3)` is
  `print cannot be used in an expression`)
- Calling any other name (`foo(3)`) is `unknown function 'foo'`

Expression statements (`x == 1` on its own line) are allowed. Their value
is discarded, except at the top level of the REPL (see REPL).

### Differences from Python

These are deliberate. Each is explained in a decision record:

| Here                                         | Python                     |
| -------------------------------------------- | -------------------------- |
| 32-bit integers that wrap                    | Unlimited integers         |
| `True`/`False` are 1/0; `print(True)` → `1`  | Separate bool; `True`      |
| No `/`                                       | `/` is true division       |
| Variables start at 0; name check is textual  | `NameError` at runtime     |
| `1 < 2 < 3` is an error                      | Chained comparison         |
| Tabs in indentation are an error             | Allowed if consistent      |

### Example

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

Output:

```
9
6
-1
3
```

### Errors

Compile errors are exactly `line N: <message>` and nothing runs. `N` is
the line where the problem is found. The compiler stops at the first
error.

| Situation                          | Message                                                |
| ---------------------------------- | ------------------------------------------------------ |
| Tab in indentation                 | `tab in indentation`                                   |
| Block has no indented lines        | `expected an indented block`                           |
| Indented line that opens no block  | `unexpected indent`                                    |
| Dedent to a level not in use       | `unindent does not match any outer indentation level` |
| Character not in the language      | `unexpected character '<c>'` or `unexpected character 0x<HH>` |
| Badly formed number                | `invalid number`                                       |
| Literal too large                  | `number too large`                                     |
| Name longer than 31 characters     | `name too long (max 31)`                               |
| Unsupported Python keyword         | `'<keyword>' is not supported`                         |
| `/` used                           | `'/' is not supported, use '//'`                       |
| Unexpected token                   | `expected <what>, got <token>`                         |
| Unknown variable                   | `unknown name '<name>'`                                |
| Unknown function                   | `unknown function '<name>'`                            |
| Wrong number of `print` arguments  | `print takes exactly 1 argument`                       |
| `print` inside an expression       | `print cannot be used in an expression`                |
| Comparison chain                   | `comparisons cannot be chained`                        |
| Too many variables                 | `too many variables (max 16)`                          |
| More than 32 nested brackets or unary operators, or more than 16 nested blocks | `too deeply nested` |
| Expression needs more than `LANG_STACK_MAX` stack cells | `expression too complex`          |
| Program too large                  | `program too large`                                    |

Unexpected characters: printable ASCII (`0x21`–`0x7E`) is shown as itself
in quotes, anything else (including bytes of UTF-8 characters such as
curly quotes) as two uppercase hex digits.

How `<token>` is shown in `expected <what>, got <token>`:

| Token                        | Shown as          |
| ---------------------------- | ----------------- |
| Operator, bracket, keyword   | `'+'`, `'while'`  |
| Name                         | `name 'foo'`      |
| Number                       | `number 12`       |
| End of line                  | `end of line`     |
| End of file                  | `end of file`     |

`<what>` is one of: a token in quotes (`':'`, `')'`, `'='`),
`expression`, `end of line`, `name`.

Runtime errors print `runtime error: <message>` and stop the program.
Messages:

- `division by zero`
- `stack overflow`, `stack underflow`, `invalid bytecode`. Compiled code
  never causes these. They protect against corrupt or hand-written
  bytecode

`invalid bytecode` covers: unknown opcode, an operand running past the end
of the code, a jump target outside the code, `pc` running off the end
without `HALT`, and a variable index `>= nvars`.

## Design

### Public API (`include/lang.h`)

```c
#define LANG_CODE_MAX    1024  /* bytes of bytecode per program */
#define LANG_STACK_MAX   16    /* data stack cells per VM */
#define LANG_VARS_MAX    16
#define LANG_NAME_MAX    31    /* characters per name */
#define LANG_NEST_MAX    32    /* nested brackets / unary operators */
#define LANG_BLOCKS_MAX  16    /* nested blocks */

typedef struct { uint8_t code[LANG_CODE_MAX]; uint16_t len; uint8_t nvars; } lang_program_t;

/* Variable names known to the compiler. Index = variable slot. */
typedef struct {
    char    name[LANG_VARS_MAX][LANG_NAME_MAX + 1];
    uint8_t count;
} lang_names_t;

typedef enum { LANG_DONE, LANG_BUDGET, LANG_ERROR } lang_status_t;

typedef struct {
    const lang_program_t *prog;
    uint16_t      pc;
    uint8_t       sp;
    lang_status_t status;  /* status of the last lang_vm_run */
    uint32_t      steps;   /* instructions run by the last lang_vm_run */
    const char   *error;   /* runtime error message (no prefix), or NULL */
    int32_t       stack[LANG_STACK_MAX];
    int32_t       vars[LANG_VARS_MAX];
} lang_vm_t;

/* Compiles src. `names` is read and extended: pass an empty table for a
 * new program, or the same table again to keep earlier variables (REPL).
 * On failure `names` is left unchanged, and "line N: message" is written
 * to err (always NUL-terminated, truncated to errlen; nothing if errlen
 * is 0). `repl` makes top-level expression statements print their value. */
bool lang_compile(const char *src, lang_names_t *names, bool repl,
                  lang_program_t *out, char *err, size_t errlen);

/* Starts prog from the beginning with all variables 0. */
void lang_vm_init(lang_vm_t *vm, const lang_program_t *prog);
/* Starts prog from the beginning, keeping current variable values. */
void lang_vm_load(lang_vm_t *vm, const lang_program_t *prog);

/* Runs at most `budget` instructions (0 = unlimited). */
lang_status_t lang_vm_run(lang_vm_t *vm, uint32_t budget);
```

Adding `LANG_WAIT` later must not need an API redesign.

### Budget rules

- One instruction = one opcode executed, including `HALT`
- The status describes the last instruction run. If `HALT` runs within
  the budget, the result is `LANG_DONE`, even if it used the last step.
  If the budget runs out first, the result is `LANG_BUDGET`, and the next
  call carries on from the same place
- Calling `lang_vm_run` after `LANG_DONE` or `LANG_ERROR` runs nothing
  and returns the same status (`steps` is 0)
- Budget 0 (unlimited) is for the Linux host and tests. Hosts that run
  several tasks must never pass 0. The ESP32 spec will enforce this

### Compile-time stack check

While emitting code, the compiler tracks how many stack cells the current
statement needs at each point. If that would exceed `LANG_STACK_MAX`, it
reports `expression too complex`. So compiled code can never overflow or
underflow the VM stack. The VM still checks every stack access.

### Port interface (`include/lang_port.h`)

```c
void lang_port_write(const char *s);  /* program output; host prints as-is */
void lang_port_error(const char *s);  /* error text; host prints as-is */
```

- `print` formats the integer itself (no `printf` in `src/`) and writes it
  with `lang_port_write`
- On a runtime error the VM sets `vm->error`, sets status `LANG_ERROR`,
  and writes `runtime error: <message>\n` with `lang_port_error`
- Compile errors are returned in `err`. The host prints them

### Bytecode

1-byte opcodes; operands inline, little-endian, read with `memcpy`.
Bytecode format version 1, recorded in `docs/bytecode.md`.

`PUSH_INT i32`, `LOAD u8`, `STORE u8`, `POP`, `ADD`, `SUB`, `MUL`,
`FLOORDIV`, `MOD`, `NEG`, `NOT`, `EQ`, `NE`, `LT`, `GT`, `LE`, `GE`,
`JUMP i16`, `JUMP_IF_FALSE i16` (always pops),
`JUMP_IF_FALSE_OR_POP i16`, `JUMP_IF_TRUE_OR_POP i16`, `PRINT`, `HALT`.

- Jump offsets are relative to the byte after the operand
- `JUMP_IF_FALSE_OR_POP`: if the top of the stack is false, jump and leave
  it there; otherwise pop it. `JUMP_IF_TRUE_OR_POP` is the same for true.
  `a and b` compiles to `a, JUMP_IF_FALSE_OR_POP end, b, end:`, which
  gives Python's operand-value result

### Files

| File                                                 | Action  | Contents                                                     |
| ---------------------------------------------------- | ------- | ------------------------------------------------------------ |
| `include/lang.h`, `include/lang_port.h`              | Create  | As above                                                     |
| `src/token.c/.h`                                     | Create  | Tokenizer, INDENT/DEDENT, bracket depth                      |
| `src/compile.c/.h`                                   | Create  | Recursive descent parser + code generation                   |
| `src/vm.c`                                           | Create  | VM loop                                                      |
| `src/opcodes.h`                                      | Create  | Opcode enum                                                  |
| `ports/linux/main.c`                                 | Create  | `tl file.py` runs a file; `tl` alone starts REPL             |
| `tests/Makefile`                                     | Create  | Builds everything, runs unit + golden tests                  |
| `tests/run_golden.sh`                                | Create  | Runs each `programs/*.py`, diffs stdout+stderr with `.expected` |
| `tests/unit/*.c`                                     | Create  | Tokenizer, compiler and VM unit tests                        |
| `tests/programs/*`                                   | Create  | Golden tests                                                 |
| `docs/language-reference.md`, `docs/bytecode.md`     | Fill in | Current language and opcodes                                 |
| `docs/decisions/0001-stack-vm.md`                    | Fill in | Why a stack VM                                               |
| `docs/decisions/0002-floor-division.md`              | Create  | Why `//` floors and `/` is not supported                     |
| `docs/decisions/0003-python-differences.md`          | Create  | The other rows of "Differences from Python", and why         |
| `docs/decisions/0004-stack-check-at-compile-time.md` | Create  | Why stack limits are a compile error                         |

Added during implementation:

| File                                                 | Contents                                                     |
| ---------------------------------------------------- | ------------------------------------------------------------ |
| `src/expr.c`                                         | Expressions, split from `compile.c` to keep files small      |
| `src/format.c/.h`                                    | Integer to text, shared by compiler and VM (no `printf`)     |
| `tests/run_repl.sh`, `tests/repl/*`                  | REPL tests: input on stdin, output with prompts              |
| `tests/unit/check.h`, `tests/unit/port_capture.c/.h` | Unit test helpers and a port that captures output            |
| `docs/decisions/0005`–`0008`                         | Choices this spec left open                                  |

### Linux host (`ports/linux/`)

- Program output goes to stdout, errors (compile and runtime) to stderr.
  `lang_port_error` flushes stdout first, so the order is right when both
  go to the same place (the golden runner uses `2>&1`)
- Exit code: 0 if the program finished, 1 on a compile or runtime error,
  2 if the file cannot be read or the arguments are wrong
- Programs run in slices of 10000 instructions. Between slices the host
  checks for Ctrl-C (POSIX signals are allowed in `ports/`, not in `src/`)

### REPL

- Prompt `>>> `. The prompt is `... ` while the input is incomplete: after
  a line ending in `:` (ignoring a trailing comment), or while a bracket
  is open
- A blank line ends a block
- Each complete input is compiled with `repl = true` and run as a new
  program with `lang_vm_load`, so **variables persist** between inputs (the
  same `lang_names_t` is passed each time)
- A top-level expression statement prints its value, as in Python
  (`>>> x` shows `5`, `>>> 2 + 3` shows `5`). Expressions inside blocks do
  not
- Errors print and the REPL continues
- Ctrl-C while a program runs stops it, prints `interrupted` and returns
  to the prompt. Ctrl-C at the prompt discards the current input. Ctrl-D
  exits

## Tests

Golden tests (each `.py` with its `.expected`, including error output):

- [x] Arithmetic and precedence, including unary minus and brackets
- [x] Floor division and modulo with negative numbers
- [x] Overflow wraparound, `-2147483648` as a literal, `-2147483648 // -1`
  and `-2147483648 % -1`
- [x] Comparisons, `and`/`or`/`not`, operand-value results (`2 and 3`),
  short-circuit (right side not run: `0 and 1 // 0` gives no error)
- [x] Variables, reassignment, variables starting at 0
- [x] `if` / `elif` / `else`, nested, and one-line blocks (`if x: print(x)`)
- [x] `while`, nested, and the example above
- [x] Indentation by 2 and 8 spaces, blank and comment lines at any
  indentation, expressions continued inside brackets, CRLF line endings,
  no final newline
- [x] Every compile error in the table, with the correct line number
  (including each token form in `got <token>`, a non-ASCII character, and
  each nesting limit)
- [x] Division by zero and modulo by zero at runtime

Unit tests:

- [x] Tokenizer emits the right INDENT/DEDENT sequence for nested and
  multi-level dedents, and none inside brackets
- [x] `x = 0` then `while True: x = x + 1` with budget 1000 returns
  `LANG_BUDGET` with `steps == 1000`; two runs of 500 leave exactly the
  same VM state (`pc`, `sp`, vars) as one run of 1000
- [x] A program whose `HALT` is the last instruction of the budget returns
  `LANG_DONE`; one step short returns `LANG_BUDGET`, then `LANG_DONE`
- [x] Calling `lang_vm_run` after `LANG_DONE` / `LANG_ERROR` returns the
  same status with `steps == 0`
- [x] Budget 0 runs a finite program to `LANG_DONE`
- [x] Hand-written bytecode: `stack overflow`, `stack underflow`, and each
  `invalid bytecode` case listed under Errors
- [x] `lang_compile` with an existing `lang_names_t` keeps earlier slots;
  a failed compile leaves `names` unchanged
- [x] `lang_vm_load` keeps variable values; `lang_vm_init` clears them
- [x] `err` is truncated and NUL-terminated when `errlen` is small

## Done when

- [x] `make -C tests` passes, built with `-std=c99 -Wall -Wextra -Wpedantic -Werror`
- [x] Tests also pass with `-fsanitize=address,undefined`
- [x] `ports/linux` REPL: `print(2 + 3 * 4)` prints `14`, `2 + 3` shows
  `5`, a `while` block typed over several lines works, variables persist
  between inputs, Ctrl-C stops `while True: pass`
- [x] `docs/language-reference.md` and `docs/bytecode.md` describe the result
- [x] Decision records 0001–0004 written

## Open questions

None. Ask before deviating from anything above.
