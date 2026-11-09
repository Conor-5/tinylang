# Language reference

The language as it is **now**. When a spec changes the language, this file
changes with it. Every example here is also a golden test in
`tests/programs/` (named under each example).

The syntax is a small subset of Python. Where this language differs from
Python, the difference is deliberate and listed in
[Differences from Python](#differences-from-python).

## A first program

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

Test: `example.py`

## Running programs

On Linux, `tl file.py` compiles and runs a file. `tl` on its own starts
the [REPL](#repl).

- Program output goes to stdout. Errors go to stderr
- Exit code: 0 if the program finished, 1 on a compile error, runtime
  error or Ctrl-C, 2 if the file cannot be read or the arguments are wrong

A program is compiled completely before it runs. If there is a compile
error, nothing runs.

## Values

There is one type: 32-bit signed integers, from `-2147483648` to
`2147483647`.

- Arithmetic wraps around on overflow: `2147483647 + 1` is `-2147483648`
- `True` is 1 and `False` is 0. `print(True)` prints `1`
- A value counts as true if it is not 0. This applies to `if`, `while`,
  `and`, `or` and `not`

### Integer literals

- Decimal only. `0` is allowed; any other literal must not start with `0`
  (`007` is `invalid number`)
- A number directly followed by a letter, `_` or `.` is `invalid number`
  (`0x10`, `1e5`, `1.5`, `1_000`, `123abc`)
- A literal must fit in 32 bits (`number too large`). The one exception:
  a minus sign directly in front of `2147483648` gives `-2147483648`.
  `-(2147483648)` is still `number too large`

## Operators

From lowest to highest precedence (the same order as Python):

| Level | Operators                  | Result                                   |
| ----- | -------------------------- | ---------------------------------------- |
| 1     | `or`                       | First true operand, else the last one    |
| 2     | `and`                      | First false operand, else the last one   |
| 3     | `not`                      | 1 if the operand is 0, else 0            |
| 4     | `==` `!=` `<` `>` `<=` `>=` | 1 or 0                                  |
| 5     | `+` `-`                    | Sum, difference (wrapping)               |
| 6     | `*` `//` `%`               | Product (wrapping), floor division, modulo |
| 7     | unary `-`                  | Negation (wrapping)                      |

Brackets `( )` group as usual.

- `//` rounds **down** (towards minus infinity), as in Python:
  `-7 // 2` is `-4`. `%` matches it: the result has the sign of the right
  operand, so `-7 % 2` is `1`
- `-2147483648 // -1` wraps to `-2147483648`. `-2147483648 % -1` is `0`
- Division or modulo by zero is a runtime error
- `and` and `or` stop as soon as the result is known: in `0 and x // 0`
  the division never runs. They give an operand's value, as in Python:
  `2 and 3` is `3`, `0 or 5` is `5`
- Comparisons cannot be chained: `1 < x < 3` is a compile error. Write
  `1 < x and x < 3`
- `/` does not exist (there are no fractions). Use `//`

```python
print(7 // 2)
print(-7 // 2)
print(-7 % 2)
print(2147483647 + 1)
print(2 and 3)
print(0 or 5)
print(not 5)
print(1 + 2 * 3)
```

Output:

```
3
-4
1
-2147483648
3
5
0
7
```

Test: `doc_operators.py`

## Statements

| Statement      | Example               |
| -------------- | --------------------- |
| Assignment     | `x = x + 1`           |
| Expression     | `x == 1` (value discarded) |
| `print`        | `print(x)`            |
| `pass`         | `pass` (does nothing) |
| `if`           | `if` / `elif` / `else` |
| `while`        | `while x > 0:`        |

### print

`print(expr)` prints the value in decimal, followed by a newline.

- It takes exactly one argument (`print takes exactly 1 argument`)
- It is a statement: `y = print(3)` is
  `print cannot be used in an expression`
- No other function exists: `foo(3)` is `unknown function 'foo'`

### Blocks and indentation

`if`, `elif`, `else` and `while` end with `:` and are followed by a block.
Indentation follows Python's rules:

- A block is indented by any number of spaces, as long as it is more than
  the line that opens it. Every line in the block uses the same
  indentation. A dedent must return to a level already in use
- A tab in the indentation of a line is an error (`tab in indentation`).
  Tabs elsewhere on a line count as spaces
- Blank lines and comment-only lines are ignored, whatever their
  indentation
- An empty block is an error (`expected an indented block`): use `pass`
- Instead of an indented block, one simple statement (assignment,
  expression, `print` or `pass`) can follow the `:` on the same line:
  `if n == 2: print(n)`. `if a: if b: pass` is an error
- Inside open brackets, newlines and indentation are ignored, so an
  expression can continue over several lines
- Line endings may be `\n` or `\r\n`; the last line needs no newline
- At most 16 blocks can be nested

```python
n = 0
while n < 3:
  n = n + 1
  if n == 2: print(n)
total = (n +
         10)
print(total)
```

Output:

```
2
13
```

Test: `doc_blocks.py`

### Comments

`#` starts a comment that runs to the end of the line.

## Names and variables

- A name starts with a letter or `_`, followed by letters, digits or `_`,
  at most 31 characters. Names are case-sensitive
- A variable is created by assigning to it. It must be assigned somewhere
  **earlier in the source text** before it can be read, otherwise the
  compile error is `unknown name 'x'`. This checks the text, not which
  lines run
- Variables start at 0
- At most 16 variables per program (per session in the REPL)

```python
if False:
    y = 1
print(y)
```

Output:

```
0
```

Test: `doc_variables.py`

### Reserved words

The keywords in use are `True`, `False`, `and`, `or`, `not`, `if`,
`elif`, `else`, `while`, `pass`, and `print`.

Every other Python keyword is reserved for later and gives
`'<keyword>' is not supported`: `None`, `as`, `assert`, `async`, `await`,
`break`, `class`, `continue`, `def`, `del`, `except`, `finally`, `for`,
`from`, `global`, `import`, `in`, `is`, `lambda`, `nonlocal`, `raise`,
`return`, `try`, `with`, `yield`.

## Limits

| Limit                                    | Value      | Error when exceeded           |
| ---------------------------------------- | ---------- | ----------------------------- |
| Bytecode per program                     | 1024 bytes | `program too large`           |
| Variables                                | 16         | `too many variables (max 16)` |
| Name length                              | 31         | `name too long (max 31)`      |
| Nested brackets and unary operators      | 32         | `too deeply nested`           |
| Nested blocks                            | 16         | `too deeply nested`           |
| Values an expression holds at once       | 16         | `expression too complex`      |

The last limit is how many partial results an expression needs at the
same time. `1+(1+(1+...))` needs one per level; `1+1+1+...` needs only
two however long it is. It is checked when compiling, so a program that
compiles can never run out of stack.

## Errors

### Compile errors

A compile error prints `line N: <message>` and nothing runs. Only the
first error is reported. For an error at the very end of the file, `N` is
the last line that has code on it.

```python
x = 1
print(x +)
```

Output:

```
line 2: expected expression, got ')'
```

Test: `doc_compile_error.py`

| Message                                              | Cause                                    |
| ---------------------------------------------------- | ---------------------------------------- |
| `tab in indentation`                                 | Tab in a line's indentation              |
| `expected an indented block`                         | `:` and newline not followed by a block  |
| `unexpected indent`                                  | Indented line that opens no block        |
| `unindent does not match any outer indentation level` | Dedent to a level not in use            |
| `unexpected character '<c>'`                         | Character not in the language            |
| `unexpected character 0x<HH>`                        | Same, for non-printable or non-ASCII bytes (e.g. curly quotes) |
| `invalid number`                                     | See [Integer literals](#integer-literals) |
| `number too large`                                   | Literal does not fit in 32 bits          |
| `name too long (max 31)`                             | Name over 31 characters                  |
| `'<keyword>' is not supported`                       | Reserved Python keyword                  |
| `'/' is not supported, use '//'`                     | `/` used                                 |
| `expected <what>, got <token>`                       | Anything else that does not fit the grammar |
| `unknown name '<name>'`                              | Variable read before any assignment      |
| `unknown function '<name>'`                          | Call to anything but `print`             |
| `print takes exactly 1 argument`                     | `print()` or `print(a, b)`               |
| `print cannot be used in an expression`              | `y = print(3)`                           |
| `comparisons cannot be chained`                      | `1 < x < 3`                              |
| `too many variables (max 16)`                        | 17th variable                            |
| `too deeply nested`                                  | See [Limits](#limits)                    |
| `expression too complex`                             | See [Limits](#limits)                    |
| `program too large`                                  | More than 1024 bytes of bytecode         |

In `expected <what>, got <token>`, `<what>` is a token in quotes (`':'`,
`')'`, `'('`), `expression` or `end of line`. `<token>` is shown as:

| Token                       | Shown as         |
| --------------------------- | ---------------- |
| Operator, bracket, keyword  | `'+'`, `'while'` |
| Name                        | `name 'foo'`     |
| Number                      | `number 12`      |
| End of line                 | `end of line`    |
| End of file                 | `end of file`    |

### Runtime errors

A runtime error prints `runtime error: <message>` and stops the program.
Output printed before the error stays.

```python
x = 0
print(1)
print(10 // x)
print(2)
```

Output:

```
1
runtime error: division by zero
```

Test: `doc_runtime_error.py`

| Message             | Cause                                                  |
| ------------------- | ------------------------------------------------------ |
| `division by zero`  | `//` or `%` by 0                                       |
| `stack overflow`    | Only from corrupt or hand-written bytecode             |
| `stack underflow`   | Only from corrupt or hand-written bytecode             |
| `invalid bytecode`  | Only from corrupt or hand-written bytecode             |

## REPL

`tl` with no file starts an interactive session:

- The prompt is `>>> `, or `... ` while the input is incomplete: after a
  line ending in `:` (a trailing comment is ignored), or while a bracket
  is open
- A blank line ends a block
- Variables keep their values between inputs
- A top-level expression shows its value, as in Python. Expressions
  inside blocks do not
- Errors are printed and the session carries on
- Ctrl-C stops a running program (`interrupted`) or, at a prompt, throws
  away the current input. Ctrl-D exits

```
>>> print(2 + 3 * 4)
14
>>> 2 + 3
5
>>> x = 5
>>> x
5
>>> while x > 3:
...     print(x)
...     x = x - 1
...
5
4
>>> x
3
```

Test: `tests/repl/basics.in` (the same input; with piped input the typed
text is not shown)

## Differences from Python

All deliberate. Each has a decision record in `docs/decisions/`.

| Here                                        | Python                   | Record |
| ------------------------------------------- | ------------------------ | ------ |
| 32-bit integers that wrap                   | Unlimited integers       | 0003   |
| `True`/`False` are 1/0; `print(True)` → `1` | Separate bool; `True`    | 0003   |
| No `/`                                      | `/` is true division     | 0002   |
| Variables start at 0; name check is textual | `NameError` at runtime   | 0003   |
| `1 < 2 < 3` is an error                     | Chained comparison       | 0003   |
| Tabs in indentation are an error            | Allowed if consistent    | 0003   |
| `print` takes one argument, is a statement  | Function, any arguments  | 0003   |
