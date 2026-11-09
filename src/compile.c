/*
 * compile.c - compiler entry point, statements and blocks, plus the
 * helpers for emitting code and reporting errors.
 *
 * Errors: the first error is recorded and `failed` is set. After that
 * every helper does nothing, the current token is TOK_ERROR or similar,
 * and the parse loops all stop, so there is never a second message.
 */
#include <string.h>

#include "compile.h"
#include "format.h"
#include "opcodes.h"

/* ---- errors -------------------------------------------------------- */

static size_t put(char *dst, size_t used, size_t n, const char *s, size_t len)
{
    while (len > 0 && *s != '\0' && used + 1 < n) {
        dst[used++] = *s++;
        len--;
    }
    dst[used] = '\0';
    return used;
}

void lang_cc_fail(lang_cc_t *c, uint32_t line, const char *pre,
                  const char *text, size_t len, const char *post)
{
    size_t n;
    if (c->failed) {
        return;
    }
    c->failed = true;
    c->err_line = line;
    n = put(c->err, 0, sizeof c->err, pre, (size_t)-1);
    n = put(c->err, n, sizeof c->err, text, len);
    put(c->err, n, sizeof c->err, post, (size_t)-1);
}

void lang_cc_error(lang_cc_t *c, const char *msg)
{
    lang_cc_fail(c, c->tok.line, msg, "", 0, "");
}

void lang_cc_error_got(lang_cc_t *c, const char *what)
{
    const lang_token_t *t = &c->tok;
    char buf[80];
    size_t n;

    if (t->kind == TOK_INDENT) {
        /* An indented line where none may start: Python's wording. */
        lang_cc_error(c, "unexpected indent");
        return;
    }
    n = put(buf, 0, sizeof buf, "expected ", (size_t)-1);
    n = put(buf, n, sizeof buf, what, (size_t)-1);
    n = put(buf, n, sizeof buf, ", got ", (size_t)-1);
    switch (t->kind) {
    case TOK_NEWLINE:
    case TOK_DEDENT:
        lang_cc_fail(c, t->line, buf, "end of line", (size_t)-1, "");
        break;
    case TOK_EOF:
        lang_cc_fail(c, t->line, buf, "end of file", (size_t)-1, "");
        break;
    case TOK_NAME:
        n = put(buf, n, sizeof buf, "name '", (size_t)-1);
        lang_cc_fail(c, t->line, buf, t->start, t->len, "'");
        break;
    case TOK_NUMBER:
        n = put(buf, n, sizeof buf, "number ", (size_t)-1);
        lang_cc_fail(c, t->line, buf, t->start, t->len, "");
        break;
    default:
        /* Operators, brackets and keywords: their text in quotes. */
        n = put(buf, n, sizeof buf, "'", (size_t)-1);
        lang_cc_fail(c, t->line, buf, t->start, t->len, "'");
        break;
    }
    (void)n;
}

/* ---- tokens -------------------------------------------------------- */

void lang_cc_advance(lang_cc_t *c)
{
    if (c->failed) {
        return;
    }
    c->tok = lang_lex_next(&c->lx);
    if (c->tok.kind == TOK_ERROR) {
        lang_cc_fail(c, c->tok.line, c->lx.error, "", 0, "");
    }
}

lang_token_t lang_cc_peek(const lang_cc_t *c)
{
    /* The lexer state is small, so a copy is the simplest lookahead. */
    lang_lexer_t copy = c->lx;
    return lang_lex_next(&copy);
}

void lang_cc_expect(lang_cc_t *c, lang_tok_kind_t kind, const char *what)
{
    if (c->failed) {
        return;
    }
    if (c->tok.kind == kind) {
        lang_cc_advance(c);
    } else {
        lang_cc_error_got(c, what);
    }
}

/* ---- code emission ------------------------------------------------- */

static void emit_byte(lang_cc_t *c, uint8_t b)
{
    if (c->failed) {
        return;
    }
    if (c->out->len >= LANG_CODE_MAX) {
        lang_cc_error(c, "program too large");
        return;
    }
    c->out->code[c->out->len++] = b;
}

/* Tracks the stack depth the code needs. Because every statement's
 * stack use is known while compiling, the limit is enforced here, and
 * compiled code can never overflow the VM stack. */
static void stack_effect(lang_cc_t *c, int effect)
{
    c->depth += effect;
    if (c->depth > LANG_STACK_MAX) {
        lang_cc_error(c, "expression too complex");
    }
}

void lang_cc_emit_op(lang_cc_t *c, uint8_t op, int effect)
{
    emit_byte(c, op);
    stack_effect(c, effect);
}

void lang_cc_emit_push(lang_cc_t *c, int32_t value)
{
    /* Operands are little-endian whatever the host's byte order. */
    uint32_t u = (uint32_t)value;
    emit_byte(c, OP_PUSH_INT);
    emit_byte(c, (uint8_t)(u & 0xFFu));
    emit_byte(c, (uint8_t)((u >> 8) & 0xFFu));
    emit_byte(c, (uint8_t)((u >> 16) & 0xFFu));
    emit_byte(c, (uint8_t)((u >> 24) & 0xFFu));
    stack_effect(c, 1);
}

void lang_cc_emit_u8(lang_cc_t *c, uint8_t op, uint8_t arg, int effect)
{
    emit_byte(c, op);
    emit_byte(c, arg);
    stack_effect(c, effect);
}

static void write_i16(lang_cc_t *c, uint16_t at, int value)
{
    /* Converting a negative int to uint16_t is defined (modulo 2^16),
     * which gives the two's complement bytes. */
    uint16_t u = (uint16_t)value;
    c->out->code[at] = (uint8_t)(u & 0xFFu);
    c->out->code[at + 1] = (uint8_t)(u >> 8);
}

uint16_t lang_cc_emit_jump(lang_cc_t *c, uint8_t op, int effect)
{
    uint16_t at;
    emit_byte(c, op);
    at = c->out->len;
    emit_byte(c, 0);
    emit_byte(c, 0);
    stack_effect(c, effect);
    return at;
}

void lang_cc_patch(lang_cc_t *c, uint16_t at)
{
    /* Offsets are relative to the byte after the 2-byte operand. With at
     * most LANG_CODE_MAX bytes of code they always fit in an int16. */
    if (c->failed) {
        return;
    }
    write_i16(c, at, (int)c->out->len - (int)(at + 2));
}

/* Emits a backward jump to `target`. */
static void emit_jump_back(lang_cc_t *c, uint8_t op, uint16_t target)
{
    uint16_t at = lang_cc_emit_jump(c, op, 0);
    if (!c->failed) {
        write_i16(c, at, (int)target - (int)(at + 2));
    }
}

/* ---- names --------------------------------------------------------- */

int lang_cc_find_name(const lang_cc_t *c, const char *s, size_t len)
{
    int i;
    for (i = 0; i < c->names.count; i++) {
        if (strlen(c->names.name[i]) == len &&
            memcmp(c->names.name[i], s, len) == 0) {
            return i;
        }
    }
    return -1;
}

/* Slot for an assignment target, adding the name if it is new. */
static int slot_for_store(lang_cc_t *c, const lang_token_t *name)
{
    int slot = lang_cc_find_name(c, name->start, name->len);
    if (slot >= 0) {
        return slot;
    }
    if (c->names.count >= LANG_VARS_MAX) {
        lang_cc_fail(c, name->line, "too many variables (max 16)", "", 0, "");
        return -1;
    }
    /* The tokenizer guarantees len <= LANG_NAME_MAX. */
    memcpy(c->names.name[c->names.count], name->start, name->len);
    c->names.name[c->names.count][name->len] = '\0';
    return c->names.count++;
}

/* ---- statements ---------------------------------------------------- */

static void statement(lang_cc_t *c);

static void print_statement(lang_cc_t *c)
{
    lang_cc_advance(c);   /* print */
    lang_cc_expect(c, TOK_LPAREN, "'('");
    if (c->failed) {
        return;
    }
    if (c->tok.kind == TOK_RPAREN) {
        lang_cc_error(c, "print takes exactly 1 argument");
        return;
    }
    lang_cc_expression(c);
    if (!c->failed && c->tok.kind == TOK_COMMA) {
        lang_cc_error(c, "print takes exactly 1 argument");
        return;
    }
    lang_cc_expect(c, TOK_RPAREN, "')'");
    lang_cc_emit_op(c, OP_PRINT, -1);
}

static void assignment(lang_cc_t *c)
{
    lang_token_t name = c->tok;
    int slot;
    lang_cc_advance(c);   /* name */
    lang_cc_advance(c);   /* = */
    /* The value is compiled before the name is added, so `x = x + 1`
     * with a new x is "unknown name 'x'", as the spec requires. */
    lang_cc_expression(c);
    slot = slot_for_store(c, &name);
    if (slot >= 0) {
        lang_cc_emit_u8(c, OP_STORE, (uint8_t)slot, -1);
    }
}

/* pass, print, assignment or expression. Does not read the NEWLINE. */
static void simple_statement(lang_cc_t *c)
{
    if (c->tok.kind == TOK_PASS) {
        lang_cc_advance(c);
    } else if (c->tok.kind == TOK_PRINT) {
        print_statement(c);
    } else if (c->tok.kind == TOK_NAME && lang_cc_peek(c).kind == TOK_ASSIGN) {
        assignment(c);
    } else {
        lang_cc_expression(c);
        /* In the REPL a top-level expression shows its value. */
        lang_cc_emit_op(c, (c->repl && c->blocks == 0) ? OP_PRINT : OP_POP, -1);
    }
}

/* ':' followed by either one simple statement on the same line, or a
 * NEWLINE and an indented block. */
static void block(lang_cc_t *c)
{
    lang_cc_expect(c, TOK_COLON, "':'");
    if (c->failed) {
        return;
    }
    c->blocks++;
    if (c->tok.kind != TOK_NEWLINE) {
        simple_statement(c);
        lang_cc_expect(c, TOK_NEWLINE, "end of line");
        c->blocks--;
        return;
    }
    lang_cc_advance(c);   /* NEWLINE */
    if (c->failed) {
        return;
    }
    if (c->tok.kind != TOK_INDENT) {
        lang_cc_error(c, "expected an indented block");
        return;
    }
    lang_cc_advance(c);   /* INDENT */
    while (!c->failed && c->tok.kind != TOK_DEDENT && c->tok.kind != TOK_EOF) {
        statement(c);
    }
    if (c->tok.kind == TOK_DEDENT) {
        lang_cc_advance(c);
    }
    c->blocks--;
}

/* Jumps to the end of an if/elif/else chain are kept as a linked list
 * through their own placeholder operands: each holds the position of
 * the previous one (NO_JUMP ends the list). This needs no array, so any
 * number of elif branches works. */
#define NO_JUMP 0xFFFFu

static uint16_t chain_jump(lang_cc_t *c, uint16_t chain)
{
    uint16_t at = lang_cc_emit_jump(c, OP_JUMP, 0);
    if (c->failed) {
        return chain;
    }
    c->out->code[at] = (uint8_t)(chain & 0xFFu);
    c->out->code[at + 1] = (uint8_t)(chain >> 8);
    return at;
}

static void patch_chain(lang_cc_t *c, uint16_t chain)
{
    while (!c->failed && chain != NO_JUMP) {
        uint16_t next = (uint16_t)(c->out->code[chain] |
                                   (c->out->code[chain + 1] << 8));
        lang_cc_patch(c, chain);
        chain = next;
    }
}

/*  if a: A  elif b: B  else: C   compiles to
 *
 *      a; JUMP_IF_FALSE L1; A; JUMP end
 *  L1: b; JUMP_IF_FALSE L2; B; JUMP end
 *  L2: C
 *  end:
 */
static void if_statement(lang_cc_t *c)
{
    uint16_t chain = NO_JUMP;
    uint16_t next_branch;

    lang_cc_advance(c);   /* if */
    lang_cc_expression(c);
    next_branch = lang_cc_emit_jump(c, OP_JUMP_IF_FALSE, -1);
    block(c);
    while (!c->failed && c->tok.kind == TOK_ELIF) {
        chain = chain_jump(c, chain);
        lang_cc_patch(c, next_branch);
        lang_cc_advance(c);   /* elif */
        lang_cc_expression(c);
        next_branch = lang_cc_emit_jump(c, OP_JUMP_IF_FALSE, -1);
        block(c);
    }
    if (!c->failed && c->tok.kind == TOK_ELSE) {
        chain = chain_jump(c, chain);
        lang_cc_patch(c, next_branch);
        lang_cc_advance(c);   /* else */
        block(c);
    } else {
        lang_cc_patch(c, next_branch);
    }
    patch_chain(c, chain);
}

/*  while a: A   compiles to   top: a; JUMP_IF_FALSE end; A; JUMP top; end: */
static void while_statement(lang_cc_t *c)
{
    uint16_t top = c->out->len;
    uint16_t exit;

    lang_cc_advance(c);   /* while */
    lang_cc_expression(c);
    exit = lang_cc_emit_jump(c, OP_JUMP_IF_FALSE, -1);
    block(c);
    emit_jump_back(c, OP_JUMP, top);
    lang_cc_patch(c, exit);
}

static void statement(lang_cc_t *c)
{
    switch (c->tok.kind) {
    case TOK_IF:
        if_statement(c);
        break;
    case TOK_WHILE:
        while_statement(c);
        break;
    case TOK_INDENT:
        lang_cc_error(c, "unexpected indent");
        break;
    default:
        simple_statement(c);
        lang_cc_expect(c, TOK_NEWLINE, "end of line");
        break;
    }
}

/* ---- entry point --------------------------------------------------- */

static void write_error(const lang_cc_t *c, char *err, size_t errlen)
{
    char num[LANG_FMT_INT_SIZE];
    size_t n;
    if (errlen == 0) {
        return;
    }
    n = put(err, 0, errlen, "line ", (size_t)-1);
    n = put(err, n, errlen, lang_fmt_uint(num, c->err_line), (size_t)-1);
    n = put(err, n, errlen, ": ", (size_t)-1);
    put(err, n, errlen, c->err, (size_t)-1);
}

bool lang_compile(const char *src, lang_names_t *names, bool repl,
                  lang_program_t *out, char *err, size_t errlen)
{
    /* static: the compiler state is a few hundred bytes, too much for a
     * small task stack. The core is not re-entrant anyway. */
    static lang_cc_t c;

    memset(&c, 0, sizeof c);
    memset(out, 0, sizeof *out);
    c.out = out;
    c.repl = repl;
    c.names = *names;
    lang_lex_init(&c.lx, src);
    lang_cc_advance(&c);

    while (!c.failed && c.tok.kind != TOK_EOF) {
        statement(&c);
    }
    emit_byte(&c, OP_HALT);

    if (c.failed) {
        write_error(&c, err, errlen);
        return false;
    }
    out->nvars = c.names.count;
    *names = c.names;
    if (errlen > 0) {
        err[0] = '\0';
    }
    return true;
}
