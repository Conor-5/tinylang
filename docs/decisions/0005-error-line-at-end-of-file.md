# 0005: Line number of errors at the end of the file

**Status:** Accepted (implementation of spec 0001; the spec did not say)

## Context

Some errors are found at the end of the file, e.g. `while x:` followed by
only blank lines, or an unclosed bracket. The end of the file is after the
last line, which may be blank or a comment. Python is not consistent here:
it reports line 1 for `if 1:\n` but the comment line for `if 1:\n# c\n`.

## Decision

An error found at the end of the file reports the **last line that has
code on it**. `x = 1\nwhile x:\n\n# comment\n` gives
`line 2: expected an indented block`.

## Why

The line that needs fixing is the last line of code. One simple rule is
easier to explain than Python's behaviour.
