# 0006: A new program has status LANG_BUDGET

**Status:** Accepted (implementation of spec 0001; the spec did not say)

## Context

`lang_vm_t.status` is "the status of the last `lang_vm_run`", and the spec
says calling `lang_vm_run` after `LANG_DONE` or `LANG_ERROR` runs nothing.
Before the first run there has been no last run, and the status enum has
only `LANG_DONE`, `LANG_BUDGET` and `LANG_ERROR`.

## Decision

`lang_vm_init` and `lang_vm_load` set the status to `LANG_BUDGET`, read as
"can run". Only `LANG_DONE` and `LANG_ERROR` are final.

## Why

`LANG_BUDGET` already means "stopped part-way, call again". A program that
has not started is the same case at `pc = 0`. This avoids adding a fourth
status value, which every host `switch` would then have to handle.
