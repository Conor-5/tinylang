/*
 * test_vm.c - unit tests for the VM: the instruction budget, status rules,
 * and hand-written bytecode for errors compiled code can never cause.
 */
#include "check.h"
#include "lang.h"
#include "opcodes.h"
#include "port_capture.h"

static lang_program_t prog;

/* Hand-written program from bytes. */
static void load(const uint8_t *code, uint16_t len, uint8_t nvars)
{
    memset(&prog, 0, sizeof prog);
    memcpy(prog.code, code, len);
    prog.len = len;
    prog.nvars = nvars;
}

static void compile_ok(const char *src)
{
    lang_names_t names;
    char err[64];
    memset(&names, 0, sizeof names);
    CHECK(lang_compile(src, &names, false, &prog, err, sizeof err));
}

#define PUSH(b0) OP_PUSH_INT, (b0), 0, 0, 0

static void test_budget_stops_and_resumes(void)
{
    lang_vm_t a, b;
    compile_ok("x = 0\nwhile True: x = x + 1\n");

    lang_vm_init(&a, &prog);
    CHECK(lang_vm_run(&a, 1000) == LANG_BUDGET);
    CHECK(a.steps == 1000);
    CHECK(a.status == LANG_BUDGET);
    CHECK(a.vars[0] > 0);

    lang_vm_init(&b, &prog);
    CHECK(lang_vm_run(&b, 500) == LANG_BUDGET);
    CHECK(b.steps == 500);
    CHECK(lang_vm_run(&b, 500) == LANG_BUDGET);
    CHECK(b.steps == 500);

    /* Two runs of 500 must end in exactly the same state as one of 1000. */
    CHECK(a.pc == b.pc);
    CHECK(a.sp == b.sp);
    CHECK(memcmp(a.vars, b.vars, sizeof a.vars) == 0);
    CHECK(memcmp(a.stack, b.stack, sizeof a.stack) == 0);

    /* And it keeps going. */
    CHECK(lang_vm_run(&a, 1000) == LANG_BUDGET);
    CHECK(a.vars[0] > b.vars[0]);
}

static void test_halt_on_last_step(void)
{
    static const uint8_t code[] = {PUSH(1), OP_POP, OP_HALT};
    lang_vm_t vm;
    load(code, sizeof code, 0);

    /* PUSH, POP, HALT = 3 instructions: HALT may use the last step. */
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 3) == LANG_DONE);
    CHECK(vm.steps == 3);

    /* One step short: BUDGET, then HALT runs on the next call. */
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 2) == LANG_BUDGET);
    CHECK(vm.steps == 2);
    CHECK(lang_vm_run(&vm, 1) == LANG_DONE);
    CHECK(vm.steps == 1);
}

static void test_run_after_done_or_error(void)
{
    static const uint8_t ok[] = {OP_HALT};
    static const uint8_t bad[] = {OP_POP, OP_HALT};
    lang_vm_t vm;

    load(ok, sizeof ok, 0);
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 0) == LANG_DONE);
    CHECK(lang_vm_run(&vm, 0) == LANG_DONE);
    CHECK(vm.steps == 0);
    CHECK(lang_vm_run(&vm, 10) == LANG_DONE);
    CHECK(vm.steps == 0);

    capture_reset();
    load(bad, sizeof bad, 0);
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 0) == LANG_ERROR);
    CHECK(lang_vm_run(&vm, 0) == LANG_ERROR);
    CHECK(vm.steps == 0);
    /* The error is reported once, not on every call. */
    CHECK_STR(capture_err, "runtime error: stack underflow\n");
}

static void test_budget_zero_runs_to_done(void)
{
    lang_vm_t vm;
    capture_reset();
    compile_ok("i = 0\nwhile i < 10000:\n    i = i + 1\nprint(i)\n");
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 0) == LANG_DONE);
    CHECK(vm.steps > 10000);
    CHECK_STR(capture_out, "10000\n");
}

/* Runs hand-written code and checks it stops with the given message. */
static void expect_error(const uint8_t *code, uint16_t len, uint8_t nvars,
                         const char *msg)
{
    lang_vm_t vm;
    char want[64];
    load(code, len, nvars);
    lang_vm_init(&vm, &prog);
    capture_reset();
    CHECK(lang_vm_run(&vm, 0) == LANG_ERROR);
    CHECK(vm.status == LANG_ERROR);
    CHECK(vm.error != NULL && strcmp(vm.error, msg) == 0);
    snprintf(want, sizeof want, "runtime error: %s\n", msg);
    CHECK_STR(capture_err, want);
}

#define EXPECT_ERROR(msg, nvars, ...)                                     \
    do {                                                                  \
        static const uint8_t code_[] = {__VA_ARGS__};                     \
        expect_error(code_, sizeof code_, (nvars), (msg));                \
    } while (0)

static void test_stack_errors(void)
{
    EXPECT_ERROR("stack overflow", 0,
                 PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1),
                 PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1),
                 PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1), OP_HALT);
    EXPECT_ERROR("stack underflow", 0, OP_POP, OP_HALT);
    EXPECT_ERROR("stack underflow", 0, PUSH(1), OP_ADD, OP_HALT);
    EXPECT_ERROR("stack underflow", 0, OP_NEG, OP_HALT);
    EXPECT_ERROR("stack underflow", 0, OP_PRINT, OP_HALT);
    EXPECT_ERROR("stack underflow", 1, OP_STORE, 0, OP_HALT);
    EXPECT_ERROR("stack underflow", 0, OP_JUMP_IF_FALSE, 0, 0, OP_HALT);
    EXPECT_ERROR("stack underflow", 0, OP_JUMP_IF_TRUE_OR_POP, 0, 0,
                 OP_HALT);
}

static void test_stack_full_is_allowed(void)
{
    static const uint8_t code[] = {
        PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1),
        PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1), PUSH(1),
        PUSH(1), PUSH(1), OP_HALT};
    lang_vm_t vm;
    load(code, sizeof code, 0);
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 0) == LANG_DONE);
    CHECK(vm.sp == LANG_STACK_MAX);
}

static void test_invalid_bytecode(void)
{
    static const uint8_t halt[] = {OP_HALT};
    lang_vm_t vm;

    /* Unknown opcodes, including 0 (zeroed memory). */
    EXPECT_ERROR("invalid bytecode", 0, 0x00);
    EXPECT_ERROR("invalid bytecode", 0, 0xFF);
    EXPECT_ERROR("invalid bytecode", 0, OP_HALT + 1);
    /* Operand running past the end of the code. */
    EXPECT_ERROR("invalid bytecode", 0, OP_PUSH_INT, 1, 2);
    EXPECT_ERROR("invalid bytecode", 0, OP_LOAD);
    EXPECT_ERROR("invalid bytecode", 0, OP_JUMP, 0);
    /* Jump targets outside the code: before the start, at and past the end. */
    EXPECT_ERROR("invalid bytecode", 0, OP_JUMP, 0xF0, 0xFF, OP_HALT);
    EXPECT_ERROR("invalid bytecode", 0, OP_JUMP, 1, 0, OP_HALT);
    EXPECT_ERROR("invalid bytecode", 0, OP_JUMP, 0x00, 0x10, OP_HALT);
    EXPECT_ERROR("invalid bytecode", 0, PUSH(0), OP_JUMP_IF_FALSE, 9, 0,
                 OP_HALT);
    EXPECT_ERROR("invalid bytecode", 0, PUSH(0),
                 OP_JUMP_IF_FALSE_OR_POP, 9, 0, OP_HALT);
    EXPECT_ERROR("invalid bytecode", 0, PUSH(1),
                 OP_JUMP_IF_TRUE_OR_POP, 9, 0, OP_HALT);
    /* pc runs off the end without HALT. */
    EXPECT_ERROR("invalid bytecode", 0, PUSH(1), OP_POP);
    /* Variable index >= nvars. */
    EXPECT_ERROR("invalid bytecode", 1, OP_LOAD, 1, OP_HALT);
    EXPECT_ERROR("invalid bytecode", 2, PUSH(1), OP_STORE, 5, OP_HALT);

    /* Program header itself is bad: nvars or len too large, no program. */
    load(halt, sizeof halt, LANG_VARS_MAX + 1);
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 0) == LANG_ERROR);
    CHECK_STR(vm.error, "invalid bytecode");

    load(halt, sizeof halt, 0);
    prog.len = LANG_CODE_MAX + 1;
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 0) == LANG_ERROR);

    capture_reset();
    lang_vm_init(&vm, NULL);
    CHECK(lang_vm_run(&vm, 0) == LANG_ERROR);
    CHECK_STR(capture_err, "runtime error: invalid bytecode\n");
}

static void test_jump_offsets(void)
{
    /* The offset is relative to the byte after the operand: JUMP +6 from
     * byte 3 lands on byte 9, skipping "PUSH 7, PRINT". */
    static const uint8_t code[] = {
        OP_JUMP, 6, 0,          /* 0..2 */
        PUSH(7),                /* 3..7 (skipped) */
        OP_PRINT,               /* 8 (skipped) */
        PUSH(2),                /* 9..13 */
        OP_PRINT,               /* 14 */
        OP_HALT                 /* 15 */
    };
    lang_vm_t vm;
    load(code, sizeof code, 0);
    capture_reset();
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 0) == LANG_DONE);
    CHECK_STR(capture_out, "2\n");
}

static void test_and_or_jumps_keep_value(void)
{
    static const uint8_t c[] = {
        PUSH(0),                          /* 0..4 */
        OP_JUMP_IF_FALSE_OR_POP, 5, 0,    /* 5..7: next 8, target 13 */
        PUSH(9),                          /* 8..12 (skipped) */
        OP_PRINT,                         /* 13: prints 0 */
        PUSH(3),                          /* 14..18 */
        OP_JUMP_IF_TRUE_OR_POP, 5, 0,     /* 19..21: next 22, target 27 */
        PUSH(9),                          /* 22..26 (skipped) */
        OP_PRINT,                         /* 27: prints 3 */
        PUSH(3),                          /* 28..32 */
        OP_JUMP_IF_FALSE_OR_POP, 5, 0,    /* 33..35: not taken, pops */
        PUSH(4),                          /* 36..40 */
        OP_PRINT,                         /* 41: prints 4 */
        OP_HALT
    };
    lang_vm_t vm;
    load(c, sizeof c, 0);
    capture_reset();
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 0) == LANG_DONE);
    CHECK_STR(capture_out, "0\n3\n4\n");
    CHECK(vm.sp == 0);
}

static void test_print_extremes(void)
{
    static const uint8_t c[] = {
        OP_PUSH_INT, 0x00, 0x00, 0x00, 0x80, OP_PRINT,   /* INT32_MIN */
        OP_PUSH_INT, 0xFF, 0xFF, 0xFF, 0x7F, OP_PRINT,   /* INT32_MAX */
        OP_PUSH_INT, 0xFF, 0xFF, 0xFF, 0xFF, OP_PRINT,   /* -1 */
        OP_HALT
    };
    lang_vm_t vm;
    load(c, sizeof c, 0);
    capture_reset();
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 0) == LANG_DONE);
    CHECK_STR(capture_out, "-2147483648\n2147483647\n-1\n");
}

static void test_load_keeps_vars_init_clears(void)
{
    lang_vm_t vm;
    lang_names_t names;
    char err[64];
    lang_program_t p2;

    memset(&names, 0, sizeof names);
    CHECK(lang_compile("a = 41\n", &names, false, &prog, err, sizeof err));
    lang_vm_init(&vm, &prog);
    CHECK(lang_vm_run(&vm, 0) == LANG_DONE);
    CHECK(vm.vars[0] == 41);

    CHECK(lang_compile("a = a + 1\n", &names, false, &p2, err, sizeof err));
    lang_vm_load(&vm, &p2);
    CHECK(vm.status == LANG_BUDGET && vm.pc == 0 && vm.sp == 0);
    CHECK(lang_vm_run(&vm, 0) == LANG_DONE);
    CHECK(vm.vars[0] == 42);

    lang_vm_init(&vm, &p2);
    CHECK(vm.vars[0] == 0);
    CHECK(vm.error == NULL);
}

int main(void)
{
    test_budget_stops_and_resumes();
    test_halt_on_last_step();
    test_run_after_done_or_error();
    test_budget_zero_runs_to_done();
    test_stack_errors();
    test_stack_full_is_allowed();
    test_invalid_bytecode();
    test_jump_offsets();
    test_and_or_jumps_keep_value();
    test_print_extremes();
    test_load_keeps_vars_init_clears();
    return check_report("test_vm");
}
