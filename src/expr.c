/*
 * expr.c - expressions: one function per precedence level, lowest first.
 *
 *   or  <  and  <  not  <  comparison  <  + -  <  * // %  <  unary -
 *
 * Binary operators at one level are parsed with a loop, not recursion,
 * so a long chain like 1+1+1+... uses no extra C stack. Recursion only
 * happens for brackets and unary operators, and those are limited to
 * LANG_NEST_MAX deep, so the parser's own stack use is bounded.
 */
#include <stdint.h>

#include "compile.h"
#include "opcodes.h"

static void or_expr(lang_cc_t *c);

/* Every bracket and unary operator counts towards the nesting limit. */
static bool nest_enter(lang_cc_t *c)
{
    c->nest++;
    if (c->nest > LANG_NEST_MAX) {
        lang_cc_error(c, "too deeply nested");
        return false;
    }
    return true;
}

static void nest_leave(lang_cc_t *c) { c->nest--; }

static void name_expr(lang_cc_t *c)
{
    lang_token_t name = c->tok;
    int slot;

    if (lang_cc_peek(c).kind == TOK_LPAREN) {
        /* Only print can be called, and it is a statement. */
        lang_cc_fail(c, name.line, "unknown function '", name.start,
                     name.len, "'");
        return;
    }
    slot = lang_cc_find_name(c, name.start, name.len);
    if (slot < 0) {
        lang_cc_fail(c, name.line, "unknown name '", name.start, name.len,
                     "'");
        return;
    }
    lang_cc_advance(c);
    lang_cc_emit_u8(c, OP_LOAD, (uint8_t)slot, 1);
}

static void primary(lang_cc_t *c)
{
    switch (c->tok.kind) {
    case TOK_NUMBER:
        if (c->tok.value > INT32_MAX) {
            /* 2147483648 is only allowed right after unary minus. */
            lang_cc_error(c, "number too large");
            return;
        }
        lang_cc_emit_push(c, (int32_t)c->tok.value);
        lang_cc_advance(c);
        break;
    case TOK_TRUE:
    case TOK_FALSE:
        lang_cc_emit_push(c, c->tok.kind == TOK_TRUE ? 1 : 0);
        lang_cc_advance(c);
        break;
    case TOK_NAME:
        name_expr(c);
        break;
    case TOK_LPAREN:
        if (nest_enter(c)) {
            lang_cc_advance(c);
            or_expr(c);
            lang_cc_expect(c, TOK_RPAREN, "')'");
        }
        nest_leave(c);
        break;
    case TOK_PRINT:
        lang_cc_error(c, "print cannot be used in an expression");
        break;
    default:
        lang_cc_error_got(c, "expression");
        break;
    }
}

static void unary(lang_cc_t *c)
{
    if (c->tok.kind != TOK_MINUS) {
        primary(c);
        return;
    }
    if (nest_enter(c)) {
        lang_cc_advance(c);   /* - */
        if (c->tok.kind == TOK_NUMBER && c->tok.value == 2147483648u) {
            /* -2147483648 cannot be written as -(2147483648) because the
             * positive value does not fit, so minus directly before this
             * one literal is compiled as the single value INT32_MIN. */
            lang_cc_emit_push(c, INT32_MIN);
            lang_cc_advance(c);
        } else {
            unary(c);
            lang_cc_emit_op(c, OP_NEG, 0);
        }
    }
    nest_leave(c);
}

static void term(lang_cc_t *c)
{
    unary(c);
    while (!c->failed) {
        uint8_t op;
        if (c->tok.kind == TOK_STAR) {
            op = OP_MUL;
        } else if (c->tok.kind == TOK_FLOORDIV) {
            op = OP_FLOORDIV;
        } else if (c->tok.kind == TOK_PERCENT) {
            op = OP_MOD;
        } else {
            break;
        }
        lang_cc_advance(c);
        unary(c);
        lang_cc_emit_op(c, op, -1);
    }
}

static void sum(lang_cc_t *c)
{
    term(c);
    while (!c->failed) {
        uint8_t op;
        if (c->tok.kind == TOK_PLUS) {
            op = OP_ADD;
        } else if (c->tok.kind == TOK_MINUS) {
            op = OP_SUB;
        } else {
            break;
        }
        lang_cc_advance(c);
        term(c);
        lang_cc_emit_op(c, op, -1);
    }
}

/* Opcode for a comparison token, or 0 if it is not one. */
static uint8_t comparison_op(lang_tok_kind_t kind)
{
    switch (kind) {
    case TOK_EQ: return OP_EQ;
    case TOK_NE: return OP_NE;
    case TOK_LT: return OP_LT;
    case TOK_GT: return OP_GT;
    case TOK_LE: return OP_LE;
    case TOK_GE: return OP_GE;
    default: return 0;
    }
}

static void comparison(lang_cc_t *c)
{
    uint8_t op;
    sum(c);
    op = comparison_op(c->tok.kind);
    if (c->failed || op == 0) {
        return;
    }
    lang_cc_advance(c);
    sum(c);
    lang_cc_emit_op(c, op, -1);
    if (!c->failed && comparison_op(c->tok.kind) != 0) {
        /* Python would read 1 < x < 3 as 1 < x and x < 3; rather than
         * silently doing something else, it is an error here. */
        lang_cc_error(c, "comparisons cannot be chained");
    }
}

static void not_expr(lang_cc_t *c)
{
    if (c->tok.kind != TOK_NOT) {
        comparison(c);
        return;
    }
    if (nest_enter(c)) {
        lang_cc_advance(c);
        not_expr(c);
        lang_cc_emit_op(c, OP_NOT, 0);
    }
    nest_leave(c);
}

/* `a and b`:  a; JUMP_IF_FALSE_OR_POP end; b; end:
 * If a is false it stays on the stack as the result; otherwise it is
 * popped and b is the result. That gives Python's operand value. For
 * the stack count, the jump is -1 (the fall-through path) and b is +1,
 * which also matches the jump path, where a is left: one value either
 * way. */
static void and_expr(lang_cc_t *c)
{
    not_expr(c);
    while (!c->failed && c->tok.kind == TOK_AND) {
        uint16_t end;
        lang_cc_advance(c);
        end = lang_cc_emit_jump(c, OP_JUMP_IF_FALSE_OR_POP, -1);
        not_expr(c);
        lang_cc_patch(c, end);
    }
}

/* `a or b`: as and_expr, with JUMP_IF_TRUE_OR_POP. */
static void or_expr(lang_cc_t *c)
{
    and_expr(c);
    while (!c->failed && c->tok.kind == TOK_OR) {
        uint16_t end;
        lang_cc_advance(c);
        end = lang_cc_emit_jump(c, OP_JUMP_IF_TRUE_OR_POP, -1);
        and_expr(c);
        lang_cc_patch(c, end);
    }
}

void lang_cc_expression(lang_cc_t *c)
{
    if (!c->failed) {
        or_expr(c);
    }
}
