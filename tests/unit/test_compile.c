/*
 * test_compile.c - unit tests for lang_compile behaviour that golden
 * tests cannot see: the names table, error buffer handling, REPL mode.
 */
#include "check.h"
#include "lang.h"
#include "port_capture.h"

static lang_program_t prog;
static char err[64];

/* Compiles and runs src to completion with the given names / repl flag,
 * keeping variables in vm. Returns false if compilation failed. */
static bool run(lang_vm_t *vm, lang_names_t *names, bool repl, const char *src)
{
    if (!lang_compile(src, names, repl, &prog, err, sizeof err)) {
        return false;
    }
    lang_vm_load(vm, &prog);
    return lang_vm_run(vm, 0) == LANG_DONE;
}

static void test_names_persist_between_compiles(void)
{
    lang_names_t names;
    lang_vm_t vm;
    memset(&names, 0, sizeof names);
    lang_vm_init(&vm, NULL);
    capture_reset();

    CHECK(run(&vm, &names, false, "a = 5\nb = 7\n"));
    CHECK(names.count == 2);
    CHECK_STR(names.name[0], "a");
    CHECK_STR(names.name[1], "b");

    /* Earlier slots are kept, new names are added after them. */
    CHECK(run(&vm, &names, false, "c = b - a\nprint(c)\n"));
    CHECK(names.count == 3);
    CHECK_STR(names.name[2], "c");
    CHECK_STR(capture_out, "2\n");
    CHECK(prog.nvars == 3);
}

static void test_failed_compile_leaves_names_unchanged(void)
{
    lang_names_t names, before;
    memset(&names, 0, sizeof names);
    CHECK(lang_compile("a = 1\n", &names, false, &prog, err, sizeof err));
    before = names;

    /* `b` and `c` would be new names, but the compile fails on line 3. */
    CHECK(!lang_compile("b = 2\nc = 3\nd = nope\n", &names, false, &prog,
                        err, sizeof err));
    CHECK_STR(err, "line 3: unknown name 'nope'");
    CHECK(memcmp(&names, &before, sizeof names) == 0);
}

static void test_error_buffer_truncated(void)
{
    lang_names_t names;
    char small[8];
    char none[1] = {'x'};
    memset(&names, 0, sizeof names);

    CHECK(!lang_compile("print(nope)\n", &names, false, &prog, small,
                        sizeof small));
    CHECK_STR(small, "line 1:");

    small[0] = 'x';
    CHECK(!lang_compile("print(nope)\n", &names, false, &prog, small, 1));
    CHECK(small[0] == '\0');

    /* errlen 0: nothing is written at all. */
    CHECK(!lang_compile("print(nope)\n", &names, false, &prog, none, 0));
    CHECK(none[0] == 'x');
}

static void test_repl_echoes_top_level_expressions(void)
{
    lang_names_t names;
    lang_vm_t vm;
    memset(&names, 0, sizeof names);
    lang_vm_init(&vm, NULL);

    capture_reset();
    CHECK(run(&vm, &names, true, "2 + 3\n"));
    CHECK_STR(capture_out, "5\n");

    capture_reset();
    CHECK(run(&vm, &names, true, "x = 4\n"));
    CHECK_STR(capture_out, "");

    capture_reset();
    CHECK(run(&vm, &names, true, "x\n"));
    CHECK_STR(capture_out, "4\n");

    /* print is a statement, not an expression: no double output. */
    capture_reset();
    CHECK(run(&vm, &names, true, "print(x)\n"));
    CHECK_STR(capture_out, "4\n");

    /* Expressions inside blocks are not echoed. */
    capture_reset();
    CHECK(run(&vm, &names, true, "if x:\n    x + 1\n    x + 2\n"));
    CHECK_STR(capture_out, "");

    /* Without repl mode nothing is echoed. */
    capture_reset();
    CHECK(run(&vm, &names, false, "x\n"));
    CHECK_STR(capture_out, "");
}

static void test_too_many_variables_across_inputs(void)
{
    lang_names_t names;
    int i;
    memset(&names, 0, sizeof names);
    for (i = 0; i < LANG_VARS_MAX; i++) {
        char src[16];
        snprintf(src, sizeof src, "v%d = 1\n", i);
        CHECK(lang_compile(src, &names, true, &prog, err, sizeof err));
    }
    CHECK(!lang_compile("w = 1\n", &names, true, &prog, err, sizeof err));
    CHECK_STR(err, "line 1: too many variables (max 16)");
    /* Existing names can still be assigned. */
    CHECK(lang_compile("v3 = 2\n", &names, true, &prog, err, sizeof err));
}

int main(void)
{
    test_names_persist_between_compiles();
    test_failed_compile_leaves_names_unchanged();
    test_error_buffer_truncated();
    test_repl_echoes_top_level_expressions();
    test_too_many_variables_across_inputs();
    return check_report("test_compile");
}
