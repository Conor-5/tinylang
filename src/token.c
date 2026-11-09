/*
 * token.c - tokenizer: turns source text into tokens, one at a time.
 *
 * Indentation follows Python: a stack holds the indentation (in spaces)
 * of every open block. At the start of each line that has code, its
 * indentation is compared with the top of the stack: deeper pushes and
 * gives INDENT, shallower pops (one DEDENT per level) and must land
 * exactly on a level already on the stack. Whether an INDENT is allowed
 * at that point is the parser's business, not the tokenizer's.
 *
 * Blank and comment-only lines produce no tokens at all, and inside
 * brackets newlines and indentation are ignored, as in Python.
 */
#include <string.h>

#include "token.h"

/* Python keywords that this language reserves but does not support yet.
 * Reserving them now means adding one later cannot break old programs. */
static const char *const unsupported_keywords[] = {
    "None", "as", "assert", "async", "await", "break", "class",
    "continue", "def", "del", "except", "finally", "for", "from",
    "global", "import", "in", "is", "lambda", "nonlocal", "raise",
    "return", "try", "with", "yield"
};

static const struct {
    const char *text;
    lang_tok_kind_t kind;
} keywords[] = {
    {"True", TOK_TRUE},   {"False", TOK_FALSE}, {"and", TOK_AND},
    {"or", TOK_OR},       {"not", TOK_NOT},     {"if", TOK_IF},
    {"elif", TOK_ELIF},   {"else", TOK_ELSE},   {"while", TOK_WHILE},
    {"pass", TOK_PASS},   {"print", TOK_PRINT}
};

#define COUNT(a) (sizeof(a) / sizeof((a)[0]))

void lang_lex_init(lang_lexer_t *lx, const char *src)
{
    memset(lx, 0, sizeof *lx);
    lx->p = src;
    lx->line = 1;
    lx->last_line = 1;
    lx->at_line_start = true;
    lx->nindents = 1;   /* indents[0] = 0: the top level */
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_name_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_name_char(char c) { return is_name_start(c) || is_digit(c); }

/* Copies s into dst (size n), stopping early if it does not fit. */
static size_t append(char *dst, size_t used, size_t n, const char *s)
{
    while (*s != '\0' && used + 1 < n) {
        dst[used++] = *s++;
    }
    dst[used] = '\0';
    return used;
}

static lang_token_t make(const lang_lexer_t *lx, lang_tok_kind_t kind,
                         const char *start, size_t len)
{
    lang_token_t t;
    t.kind = kind;
    t.line = lx->line;
    t.start = start;
    t.len = (uint16_t)len;
    t.value = 0;
    return t;
}

static lang_token_t fail(lang_lexer_t *lx, const char *msg)
{
    if (lx->error[0] == '\0') {
        append(lx->error, 0, sizeof lx->error, msg);
    }
    return make(lx, TOK_ERROR, lx->p, 0);
}

static lang_token_t fail_char(lang_lexer_t *lx, unsigned char c)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[8];
    size_t n = append(lx->error, 0, sizeof lx->error, "unexpected character ");
    if (c >= 0x21 && c <= 0x7E) {
        buf[0] = '\'';
        buf[1] = (char)c;
        buf[2] = '\'';
        buf[3] = '\0';
    } else {
        buf[0] = '0';
        buf[1] = 'x';
        buf[2] = hex[c >> 4];
        buf[3] = hex[c & 0x0F];
        buf[4] = '\0';
    }
    append(lx->error, n, sizeof lx->error, buf);
    return make(lx, TOK_ERROR, lx->p, 0);
}

/* True if p is at the end of a line: "\n", "\r\n" or the end of the text. */
static bool at_eol(const char *p)
{
    return *p == '\n' || *p == '\0' || (p[0] == '\r' && p[1] == '\n');
}

static void skip_eol(lang_lexer_t *lx)
{
    if (*lx->p == '\r') {
        lx->p++;
    }
    if (*lx->p == '\n') {
        lx->p++;
        lx->line++;
    }
}

/* Reads the indentation of the next line that contains code, skipping
 * blank and comment-only lines. Sets pending INDENT/DEDENT state.
 * Returns TOK_NEWLINE as "nothing to report", TOK_INDENT, or TOK_ERROR. */
static lang_token_t read_indentation(lang_lexer_t *lx)
{
    for (;;) {
        const char *q = lx->p;
        uint32_t col = 0;
        bool tab = false;
        while (*q == ' ' || *q == '\t') {
            if (*q == '\t') {
                tab = true;
            }
            /* Cap the count: anything this deep is an error anyway, and
             * the cap keeps col from overflowing on absurd input. */
            if (col < 0xFFFF) {
                col++;
            }
            q++;
        }
        if (*q == '#') {
            while (!at_eol(q)) {
                q++;
            }
        }
        if (at_eol(q)) {
            /* Blank or comment-only line: its indentation means nothing. */
            lx->p = q;
            if (*q == '\0') {
                return make(lx, TOK_NEWLINE, q, 0);
            }
            skip_eol(lx);
            continue;
        }
        lx->p = q;
        lx->at_line_start = false;
        if (tab) {
            return fail(lx, "tab in indentation");
        }
        if (col > lx->indents[lx->nindents - 1]) {
            if (lx->nindents > LANG_BLOCKS_MAX) {
                return fail(lx, "too deeply nested");
            }
            lx->indents[lx->nindents++] = (uint16_t)col;
            return make(lx, TOK_INDENT, q, 0);
        }
        while (col < lx->indents[lx->nindents - 1]) {
            lx->nindents--;
            lx->pending_dedents++;
        }
        if (col != lx->indents[lx->nindents - 1]) {
            return fail(lx, "unindent does not match any outer indentation level");
        }
        return make(lx, TOK_NEWLINE, q, 0);
    }
}

static lang_token_t lex_number(lang_lexer_t *lx)
{
    const char *start = lx->p;
    const char *q = start;
    uint32_t value = 0;
    bool too_large = false;
    lang_token_t t;

    while (is_digit(*q)) {
        uint32_t d = (uint32_t)(*q - '0');
        /* 2147483648 is the largest value the tokenizer accepts; the
         * parser allows it only directly after unary minus. */
        if (value > (2147483648u - d) / 10u) {
            too_large = true;
        } else {
            value = value * 10u + d;
        }
        q++;
    }
    if (is_name_char(*q) || *q == '.' || (start[0] == '0' && q - start > 1)) {
        lx->p = q;
        return fail(lx, "invalid number");
    }
    if (too_large) {
        lx->p = q;
        return fail(lx, "number too large");
    }
    t = make(lx, TOK_NUMBER, start, (size_t)(q - start));
    t.value = value;
    lx->p = q;
    return t;
}

static lang_token_t lex_name(lang_lexer_t *lx)
{
    const char *start = lx->p;
    const char *q = start;
    size_t len;
    size_t i;

    while (is_name_char(*q)) {
        q++;
    }
    len = (size_t)(q - start);
    lx->p = q;
    for (i = 0; i < COUNT(keywords); i++) {
        if (strlen(keywords[i].text) == len &&
            memcmp(keywords[i].text, start, len) == 0) {
            return make(lx, keywords[i].kind, start, len);
        }
    }
    for (i = 0; i < COUNT(unsupported_keywords); i++) {
        const char *kw = unsupported_keywords[i];
        if (strlen(kw) == len && memcmp(kw, start, len) == 0) {
            size_t n = append(lx->error, 0, sizeof lx->error, "'");
            n = append(lx->error, n, sizeof lx->error, kw);
            append(lx->error, n, sizeof lx->error, "' is not supported");
            return make(lx, TOK_ERROR, start, 0);
        }
    }
    if (len > LANG_NAME_MAX) {
        return fail(lx, "name too long (max 31)");
    }
    return make(lx, TOK_NAME, start, len);
}

/* Operators and punctuation. Two-character operators are tried first. */
static lang_token_t lex_operator(lang_lexer_t *lx)
{
    static const struct {
        char text[3];
        lang_tok_kind_t kind;
    } ops[] = {
        {"//", TOK_FLOORDIV}, {"==", TOK_EQ}, {"!=", TOK_NE},
        {"<=", TOK_LE},       {">=", TOK_GE}, {"+", TOK_PLUS},
        {"-", TOK_MINUS},     {"*", TOK_STAR}, {"%", TOK_PERCENT},
        {"<", TOK_LT},        {">", TOK_GT},  {"=", TOK_ASSIGN},
        {"(", TOK_LPAREN},    {")", TOK_RPAREN}, {",", TOK_COMMA},
        {":", TOK_COLON}
    };
    const char *start = lx->p;
    size_t i;

    for (i = 0; i < COUNT(ops); i++) {
        size_t len = ops[i].text[1] == '\0' ? 1 : 2;
        if (strncmp(start, ops[i].text, len) == 0) {
            lx->p += len;
            if (ops[i].kind == TOK_LPAREN) {
                lx->brackets++;
            } else if (ops[i].kind == TOK_RPAREN && lx->brackets > 0) {
                lx->brackets--;
            }
            return make(lx, ops[i].kind, start, len);
        }
    }
    if (*start == '/') {
        return fail(lx, "'/' is not supported, use '//'");
    }
    return fail_char(lx, (unsigned char)*start);
}

/* Produces the next token, ignoring the bookkeeping of last_line. */
static lang_token_t next(lang_lexer_t *lx)
{
    if (lx->error[0] != '\0') {
        return make(lx, TOK_ERROR, lx->p, 0);
    }
    if (lx->pending_dedents > 0) {
        lx->pending_dedents--;
        return make(lx, TOK_DEDENT, lx->p, 0);
    }
    if (lx->at_line_start && lx->brackets == 0) {
        lang_token_t t = read_indentation(lx);
        if (t.kind != TOK_NEWLINE) {
            return t;
        }
        if (lx->pending_dedents > 0) {
            lx->pending_dedents--;
            return make(lx, TOK_DEDENT, lx->p, 0);
        }
    }
    for (;;) {
        char c = *lx->p;
        if (c == ' ' || c == '\t') {
            lx->p++;
        } else if (c == '#') {
            while (!at_eol(lx->p)) {
                lx->p++;
            }
        } else if (at_eol(lx->p)) {
            if (c == '\0') {
                break;
            }
            if (lx->brackets > 0) {
                skip_eol(lx);   /* inside brackets a newline is just space */
                continue;
            }
            /* End of a line with code on it. */
            {
                lang_token_t t = make(lx, TOK_NEWLINE, lx->p, 0);
                skip_eol(lx);
                lx->at_line_start = true;
                lx->need_newline = false;
                return t;
            }
        } else {
            break;
        }
    }
    if (*lx->p == '\0') {
        /* End of text: finish the last line, close every open block.
         * With a bracket still open the line never ended, so no NEWLINE:
         * the parser then reports "got end of file". */
        if (lx->need_newline && lx->brackets == 0) {
            lx->need_newline = false;
            return make(lx, TOK_NEWLINE, lx->p, 0);
        }
        if (lx->nindents > 1) {
            lx->nindents--;
            return make(lx, TOK_DEDENT, lx->p, 0);
        }
        return make(lx, TOK_EOF, lx->p, 0);
    }
    lx->need_newline = true;
    if (is_digit(*lx->p)) {
        return lex_number(lx);
    }
    if (is_name_start(*lx->p)) {
        return lex_name(lx);
    }
    return lex_operator(lx);
}

lang_token_t lang_lex_next(lang_lexer_t *lx)
{
    lang_token_t t = next(lx);
    /* Tokens made at the end of the text (the final NEWLINE, DEDENTs and
     * EOF) report the last line that had code on it, so an error there
     * points at real code rather than past the end of the file. */
    if (*t.start == '\0') {
        t.line = lx->last_line;
    } else {
        lx->last_line = t.line;
    }
    return t;
}
