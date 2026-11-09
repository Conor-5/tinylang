/*
 * compile.h - compiler state and the helpers shared by compile.c
 * (statements, code emission, errors) and expr.c (expressions).
 *
 * The compiler is a single pass: a recursive descent parser that emits
 * bytecode as it goes, with no syntax tree. Internal to the core.
 */
#ifndef COMPILE_H
#define COMPILE_H

#include <stdbool.h>
#include <stdint.h>

#include "lang.h"
#include "token.h"

typedef struct {
    lang_lexer_t    lx;
    lang_token_t    tok;      /* current token */
    lang_names_t    names;    /* working copy, copied back on success */
    lang_program_t *out;
    bool            repl;     /* echo top-level expression statements */
    bool            failed;   /* first error found; everything else stops */
    uint32_t        err_line;
    char            err[80];
    int             depth;    /* stack cells in use at this point */
    int             nest;     /* open brackets + unary operators */
    int             blocks;   /* open blocks (0 = top level) */
} lang_cc_t;

/* Moves to the next token. A tokenizer error becomes the compile error. */
void lang_cc_advance(lang_cc_t *c);

/* Next token after the current one, without moving. */
lang_token_t lang_cc_peek(const lang_cc_t *c);

/* If the current token is `kind`, moves past it; otherwise reports
 * "expected <what>, got <token>". */
void lang_cc_expect(lang_cc_t *c, lang_tok_kind_t kind, const char *what);

/* Reports an error at `line`: pre + text[0..len) + post. Only the first
 * error is kept. */
void lang_cc_fail(lang_cc_t *c, uint32_t line, const char *pre,
                  const char *text, size_t len, const char *post);

/* Reports msg at the current token's line. */
void lang_cc_error(lang_cc_t *c, const char *msg);

/* Reports "expected <what>, got <current token>". */
void lang_cc_error_got(lang_cc_t *c, const char *what);

/* Emits an opcode that changes the stack depth by `effect` cells. */
void lang_cc_emit_op(lang_cc_t *c, uint8_t op, int effect);
void lang_cc_emit_push(lang_cc_t *c, int32_t value);
void lang_cc_emit_u8(lang_cc_t *c, uint8_t op, uint8_t arg, int effect);

/* Emits a forward jump with a placeholder offset; returns where the
 * offset is, for lang_cc_patch. */
uint16_t lang_cc_emit_jump(lang_cc_t *c, uint8_t op, int effect);

/* Points the jump whose offset is at `at` to the current end of code. */
void lang_cc_patch(lang_cc_t *c, uint16_t at);

/* Slot of a known variable name, or -1. */
int lang_cc_find_name(const lang_cc_t *c, const char *s, size_t len);

/* Compiles one expression (expr.c). Leaves one value on the stack. */
void lang_cc_expression(lang_cc_t *c);

#endif /* COMPILE_H */
