/*
 * token.h - tokenizer: turns source text into tokens, one at a time.
 *
 * Besides ordinary tokens it produces NEWLINE at the end of each logical
 * line and INDENT / DEDENT when indentation changes, so the parser can
 * treat blocks like brackets. Internal to the core, not public API.
 */
#ifndef TOKEN_H
#define TOKEN_H

#include <stdbool.h>
#include <stdint.h>

#include "lang.h"

typedef enum {
    TOK_EOF,
    TOK_NEWLINE,
    TOK_INDENT,
    TOK_DEDENT,
    TOK_NAME,
    TOK_NUMBER,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_COMMA,
    TOK_COLON,
    TOK_ASSIGN,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_FLOORDIV,
    TOK_PERCENT,
    TOK_EQ,
    TOK_NE,
    TOK_LT,
    TOK_GT,
    TOK_LE,
    TOK_GE,
    /* keywords */
    TOK_TRUE,
    TOK_FALSE,
    TOK_AND,
    TOK_OR,
    TOK_NOT,
    TOK_IF,
    TOK_ELIF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_PASS,
    TOK_PRINT,
    /* the lexer found an error; the message is in lang_lexer_t.error */
    TOK_ERROR
} lang_tok_kind_t;

typedef struct {
    lang_tok_kind_t kind;
    uint32_t    line;    /* line the token starts on (1-based) */
    const char *start;   /* token text in the source (not NUL-terminated) */
    uint16_t    len;
    uint32_t    value;   /* TOK_NUMBER: 0 .. 2147483648 */
} lang_token_t;

typedef struct {
    const char *p;            /* next character to read */
    uint32_t line;            /* line of *p */
    uint32_t last_line;       /* line of the last token returned */
    bool     at_line_start;   /* indentation of the next line not read yet */
    uint32_t brackets;        /* open brackets: newlines are ignored inside */
    uint8_t  pending_dedents; /* DEDENT tokens still to return */
    bool     need_newline;    /* current line has tokens: a NEWLINE is owed */
    uint8_t  nindents;        /* entries in indents[] (always >= 1) */
    uint16_t indents[LANG_BLOCKS_MAX + 1];
    char     error[64];       /* message for TOK_ERROR */
} lang_lexer_t;

void lang_lex_init(lang_lexer_t *lx, const char *src);

/* Returns the next token. After TOK_ERROR or TOK_EOF it keeps returning
 * the same kind. Never reads past the terminating NUL. */
lang_token_t lang_lex_next(lang_lexer_t *lx);

#endif /* TOKEN_H */
