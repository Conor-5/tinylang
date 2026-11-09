/*
 * test_token.c - unit tests for the tokenizer, mainly the INDENT / DEDENT
 * sequence, which golden tests can only check indirectly.
 */
#include "check.h"
#include "token.h"

/* Lexes src and checks the token kinds match want[] exactly (ending with
 * TOK_EOF). Returns 1 if they all matched. */
static int kinds_are(const char *src, const lang_tok_kind_t *want, int n)
{
    lang_lexer_t lx;
    int i;
    lang_lex_init(&lx, src);
    for (i = 0; i < n; i++) {
        lang_token_t t = lang_lex_next(&lx);
        if (t.kind != want[i]) {
            printf("  token %d: got kind %d, want %d\n", i, (int)t.kind,
                   (int)want[i]);
            return 0;
        }
    }
    return 1;
}

#define KINDS(src, ...)                                                   \
    do {                                                                  \
        static const lang_tok_kind_t want_[] = {__VA_ARGS__};             \
        CHECK(kinds_are((src), want_,                                     \
                        (int)(sizeof want_ / sizeof want_[0])));          \
    } while (0)

static void test_nested_and_multi_level_dedent(void)
{
    KINDS("a\n"
          "if a:\n"
          "    b\n"
          "    if b:\n"
          "        c\n"
          "d\n",
          TOK_NAME, TOK_NEWLINE,
          TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE,
          TOK_INDENT, TOK_NAME, TOK_NEWLINE,
          TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE,
          TOK_INDENT, TOK_NAME, TOK_NEWLINE,
          TOK_DEDENT, TOK_DEDENT,
          TOK_NAME, TOK_NEWLINE, TOK_EOF);
}

static void test_dedent_to_middle_level(void)
{
    KINDS("if a:\n"
          "  if b:\n"
          "      c\n"
          "  d\n",
          TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE,
          TOK_INDENT, TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE,
          TOK_INDENT, TOK_NAME, TOK_NEWLINE,
          TOK_DEDENT, TOK_NAME, TOK_NEWLINE,
          TOK_DEDENT, TOK_EOF);
}

static void test_dedents_at_eof_without_newline(void)
{
    KINDS("if a:\n"
          "    if b:\n"
          "        c",
          TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE,
          TOK_INDENT, TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE,
          TOK_INDENT, TOK_NAME, TOK_NEWLINE,
          TOK_DEDENT, TOK_DEDENT, TOK_EOF);
}

static void test_blank_and_comment_lines_ignored(void)
{
    KINDS("if a:\n"
          "\n"
          "        # comment\n"
          "    b\n"
          "   \n"
          "c  # tail\n",
          TOK_IF, TOK_NAME, TOK_COLON, TOK_NEWLINE,
          TOK_INDENT, TOK_NAME, TOK_NEWLINE,
          TOK_DEDENT, TOK_NAME, TOK_NEWLINE, TOK_EOF);
}

static void test_no_indent_inside_brackets(void)
{
    KINDS("x = (1,\n"
          "        2\n"
          "  )\n"
          "y\n",
          TOK_NAME, TOK_ASSIGN, TOK_LPAREN, TOK_NUMBER, TOK_COMMA,
          TOK_NUMBER, TOK_RPAREN, TOK_NEWLINE,
          TOK_NAME, TOK_NEWLINE, TOK_EOF);
}

static void test_operators_and_keywords(void)
{
    KINDS("+ - * // % == != < > <= >= = ( ) , :"
          " True False and or not if elif else while pass print _x",
          TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_FLOORDIV, TOK_PERCENT,
          TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE, TOK_ASSIGN,
          TOK_LPAREN, TOK_RPAREN, TOK_COMMA, TOK_COLON,
          TOK_TRUE, TOK_FALSE, TOK_AND, TOK_OR, TOK_NOT, TOK_IF,
          TOK_ELIF, TOK_ELSE, TOK_WHILE, TOK_PASS, TOK_PRINT, TOK_NAME,
          TOK_NEWLINE, TOK_EOF);
}

static void test_numbers_and_lines(void)
{
    lang_lexer_t lx;
    lang_token_t t;
    lang_lex_init(&lx, "0 2147483648\r\n\n  # c\n42");
    t = lang_lex_next(&lx);
    CHECK(t.kind == TOK_NUMBER && t.value == 0 && t.line == 1);
    t = lang_lex_next(&lx);
    CHECK(t.kind == TOK_NUMBER && t.value == 2147483648u && t.len == 10);
    t = lang_lex_next(&lx);
    CHECK(t.kind == TOK_NEWLINE);
    t = lang_lex_next(&lx);
    CHECK(t.kind == TOK_NUMBER && t.value == 42 && t.line == 4);
}

static void test_error_sticks(void)
{
    lang_lexer_t lx;
    lang_token_t t;
    lang_lex_init(&lx, "x @ y\n");
    t = lang_lex_next(&lx);
    CHECK(t.kind == TOK_NAME);
    t = lang_lex_next(&lx);
    CHECK(t.kind == TOK_ERROR);
    CHECK_STR(lx.error, "unexpected character '@'");
    t = lang_lex_next(&lx);
    CHECK(t.kind == TOK_ERROR);
}

int main(void)
{
    test_nested_and_multi_level_dedent();
    test_dedent_to_middle_level();
    test_dedents_at_eof_without_newline();
    test_blank_and_comment_lines_ignored();
    test_no_indent_inside_brackets();
    test_operators_and_keywords();
    test_numbers_and_lines();
    test_error_sticks();
    return check_report("test_token");
}
