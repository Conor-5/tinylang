# 0003: Where the language differs from Python

**Status:** Accepted (spec 0001)

## Context

The language uses Python syntax so it is easy to pick up. A microcontroller
VM cannot match Python everywhere, and some Python behaviour works against
predictable programs. Each difference below is deliberate.

## Decisions

**32-bit integers that wrap.** Python integers grow without limit, which
needs heap memory. Here they are 32-bit and wrap on overflow
(`2147483647 + 1` is `-2147483648`). This is fixed, predictable, and what
the hardware does.

**No bool type: `True` is 1, `False` is 0.** `print(True)` prints `1`. A
separate type would need tagged values in the VM. Comparisons and `not`
give 1 or 0. `and`/`or` give an operand's value, as in Python.

**Variables start at 0, and the name check is textual.** A variable must be
assigned somewhere earlier in the source text before it is read. The
compiler checks this; it does not know which lines will run, so

```python
if False:
    y = 1
print(y)
```

compiles and prints `0` (Python raises `NameError`). The check is neither
complete nor sound, but it catches typos at compile time, which on a
device is far better than a runtime error, and costs nothing at run time.

**No chained comparisons.** In Python `1 < x < 3` means
`1 < x and x < 3`. Here it is the error `comparisons cannot be chained`,
rather than silently computing `(1 < x) < 3`.

**Tabs in indentation are an error.** Python allows tabs if used
consistently, but editors show them at different widths, and Python's rules
for mixing them are hard to explain. Any number of spaces is allowed.

**`print` is a statement with exactly one argument.** It cannot be used in
an expression, because it returns nothing.

**Every other Python keyword is reserved.** `for`, `def`, `return` and so
on give `'for' is not supported`, a clearer message than a parse error,
and a later spec can add them without breaking programs that used them as
names.

## Consequences

CPython differential testing must account for wraparound and for `1` vs
`True` in output. Everything else that compiles here behaves as in Python.
