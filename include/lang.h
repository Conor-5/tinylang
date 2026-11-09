/*
 * lang.h - public API of the language core.
 *
 * A host compiles source text to a program with lang_compile(), then runs
 * it on a VM with lang_vm_run(), a limited number of instructions at a
 * time. All sizes are fixed here; the core never allocates memory.
 */
#ifndef LANG_H
#define LANG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LANG_CODE_MAX    1024  /* bytes of bytecode per program */
#define LANG_STACK_MAX   16    /* data stack cells per VM */
#define LANG_VARS_MAX    16    /* variables per program / REPL session */
#define LANG_NAME_MAX    31    /* characters per name */
#define LANG_NEST_MAX    32    /* nested brackets / unary operators */
#define LANG_BLOCKS_MAX  16    /* nested blocks */

typedef struct {
    uint8_t  code[LANG_CODE_MAX];
    uint16_t len;
    uint8_t  nvars;
} lang_program_t;

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
 * is 0). `repl` makes top-level expression statements print their value.
 * Not re-entrant: the compiler keeps its state in static memory, so only
 * one compile may run at a time. */
bool lang_compile(const char *src, lang_names_t *names, bool repl,
                  lang_program_t *out, char *err, size_t errlen);

/* Starts prog from the beginning with all variables 0. */
void lang_vm_init(lang_vm_t *vm, const lang_program_t *prog);

/* Starts prog from the beginning, keeping current variable values. */
void lang_vm_load(lang_vm_t *vm, const lang_program_t *prog);

/* Runs at most `budget` instructions (0 = unlimited). */
lang_status_t lang_vm_run(lang_vm_t *vm, uint32_t budget);

#endif /* LANG_H */
