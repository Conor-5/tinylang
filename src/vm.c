/*
 * vm.c - the stack VM: runs a compiled program a limited number of
 * instructions at a time.
 *
 * The VM trusts nothing about the bytecode. Compiled code never trips
 * these checks, but bytecode may one day come from elsewhere (stored or
 * sent to a device), so every operand read, jump target, variable index
 * and stack access is checked, and a bad one stops the program with
 * "invalid bytecode" or a stack error instead of undefined behaviour.
 *
 * Arithmetic is done in uint32_t, where overflow is defined to wrap, and
 * converted back to int32_t without relying on implementation-defined
 * conversions.
 */
#include <string.h>

#include "format.h"
#include "lang.h"
#include "lang_port.h"
#include "opcodes.h"

/* uint32_t -> int32_t two's complement, without implementation-defined
 * behaviour: values above INT32_MAX are mapped down explicitly. */
static int32_t to_i32(uint32_t u)
{
    if (u <= (uint32_t)INT32_MAX) {
        return (int32_t)u;
    }
    return -(int32_t)(~u) - 1;
}

static void put_str(char *dst, size_t *used, size_t n, const char *s)
{
    while (*s != '\0' && *used + 1 < n) {
        dst[(*used)++] = *s++;
    }
    dst[*used] = '\0';
}

/* Stops the program with a runtime error and reports it once. */
static lang_status_t fail(lang_vm_t *vm, const char *msg)
{
    char line[48];
    size_t n = 0;
    vm->error = msg;
    vm->status = LANG_ERROR;
    put_str(line, &n, sizeof line, "runtime error: ");
    put_str(line, &n, sizeof line, msg);
    put_str(line, &n, sizeof line, "\n");
    lang_port_error(line);
    return LANG_ERROR;
}

static void reset(lang_vm_t *vm, const lang_program_t *prog)
{
    vm->prog = prog;
    vm->pc = 0;
    vm->sp = 0;
    /* LANG_BUDGET also means "ready to run": only DONE and ERROR are
     * final, so a fresh program starts in the resumable state. */
    vm->status = LANG_BUDGET;
    vm->steps = 0;
    vm->error = NULL;
    memset(vm->stack, 0, sizeof vm->stack);
}

void lang_vm_init(lang_vm_t *vm, const lang_program_t *prog)
{
    reset(vm, prog);
    memset(vm->vars, 0, sizeof vm->vars);
}

void lang_vm_load(lang_vm_t *vm, const lang_program_t *prog)
{
    reset(vm, prog);
}

/* Little-endian operands, assembled byte by byte so the result does not
 * depend on the host's byte order or alignment. Caller checks bounds. */
static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static int32_t read_i16(const uint8_t *p)
{
    uint32_t u = (uint32_t)p[0] | ((uint32_t)p[1] << 8);
    return u < 0x8000u ? (int32_t)u : (int32_t)u - 0x10000;
}

/* Python-style floor division: rounds toward minus infinity. b != 0. */
static int32_t floor_div(int32_t a, int32_t b)
{
    int32_t q;
    if (a == INT32_MIN && b == -1) {
        return INT32_MIN;   /* wraps; C's / would be undefined here */
    }
    q = a / b;
    if (a % b != 0 && ((a < 0) != (b < 0))) {
        q -= 1;             /* C truncates toward zero */
    }
    return q;
}

/* Python-style modulo: result has the sign of b. b != 0. */
static int32_t floor_mod(int32_t a, int32_t b)
{
    int32_t r;
    if (b == -1) {
        return 0;           /* also avoids INT32_MIN % -1, undefined in C */
    }
    r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) {
        r += b;
    }
    return r;
}

static int32_t binary(uint8_t op, int32_t a, int32_t b)
{
    switch (op) {
    case OP_ADD: return to_i32((uint32_t)a + (uint32_t)b);
    case OP_SUB: return to_i32((uint32_t)a - (uint32_t)b);
    case OP_MUL: return to_i32((uint32_t)a * (uint32_t)b);
    case OP_FLOORDIV: return floor_div(a, b);
    case OP_MOD: return floor_mod(a, b);
    case OP_EQ: return a == b;
    case OP_NE: return a != b;
    case OP_LT: return a < b;
    case OP_GT: return a > b;
    case OP_LE: return a <= b;
    default: return a >= b;   /* OP_GE */
    }
}

static void print_value(int32_t v)
{
    char buf[LANG_FMT_INT_SIZE + 1];
    size_t n;
    lang_fmt_int(buf, v);
    n = strlen(buf);
    buf[n] = '\n';
    buf[n + 1] = '\0';
    lang_port_write(buf);
}

/* Runs one instruction. Returns LANG_BUDGET to keep going. */
static lang_status_t step(lang_vm_t *vm)
{
    const lang_program_t *prog = vm->prog;
    const uint8_t *code = prog->code;
    uint32_t len = prog->len;
    uint32_t pc = vm->pc;
    uint8_t op;

    if (pc >= len) {
        return fail(vm, "invalid bytecode");   /* ran off the end */
    }
    op = code[pc++];

    switch (op) {
    case OP_PUSH_INT:
        if (pc + 4 > len) {
            return fail(vm, "invalid bytecode");
        }
        if (vm->sp >= LANG_STACK_MAX) {
            return fail(vm, "stack overflow");
        }
        vm->stack[vm->sp++] = to_i32(read_u32(code + pc));
        pc += 4;
        break;

    case OP_LOAD:
    case OP_STORE: {
        uint8_t slot;
        if (pc + 1 > len) {
            return fail(vm, "invalid bytecode");
        }
        slot = code[pc++];
        if (slot >= prog->nvars) {
            return fail(vm, "invalid bytecode");
        }
        if (op == OP_LOAD) {
            if (vm->sp >= LANG_STACK_MAX) {
                return fail(vm, "stack overflow");
            }
            vm->stack[vm->sp++] = vm->vars[slot];
        } else {
            if (vm->sp < 1) {
                return fail(vm, "stack underflow");
            }
            vm->vars[slot] = vm->stack[--vm->sp];
        }
        break;
    }

    case OP_POP:
        if (vm->sp < 1) {
            return fail(vm, "stack underflow");
        }
        vm->sp--;
        break;

    case OP_ADD: case OP_SUB: case OP_MUL: case OP_FLOORDIV: case OP_MOD:
    case OP_EQ: case OP_NE: case OP_LT: case OP_GT: case OP_LE: case OP_GE: {
        int32_t a, b;
        if (vm->sp < 2) {
            return fail(vm, "stack underflow");
        }
        a = vm->stack[vm->sp - 2];
        b = vm->stack[vm->sp - 1];
        if ((op == OP_FLOORDIV || op == OP_MOD) && b == 0) {
            return fail(vm, "division by zero");
        }
        vm->sp--;
        vm->stack[vm->sp - 1] = binary(op, a, b);
        break;
    }

    case OP_NEG:
    case OP_NOT:
        if (vm->sp < 1) {
            return fail(vm, "stack underflow");
        }
        if (op == OP_NEG) {
            /* 0 - x in unsigned arithmetic: -INT32_MIN wraps to itself. */
            vm->stack[vm->sp - 1] = to_i32(0u - (uint32_t)vm->stack[vm->sp - 1]);
        } else {
            vm->stack[vm->sp - 1] = vm->stack[vm->sp - 1] == 0;
        }
        break;

    case OP_JUMP:
    case OP_JUMP_IF_FALSE:
    case OP_JUMP_IF_FALSE_OR_POP:
    case OP_JUMP_IF_TRUE_OR_POP: {
        int32_t target;
        bool jump = true;
        if (pc + 2 > len) {
            return fail(vm, "invalid bytecode");
        }
        /* The offset counts from the byte after the operand. */
        target = (int32_t)(pc + 2) + read_i16(code + pc);
        pc += 2;
        if (op != OP_JUMP && vm->sp < 1) {
            return fail(vm, "stack underflow");
        }
        /* Checked whether or not the jump is taken: a bad target is bad
         * bytecode even on a path that has not run yet. */
        if (target < 0 || (uint32_t)target >= len) {
            return fail(vm, "invalid bytecode");
        }
        if (op == OP_JUMP_IF_FALSE) {
            jump = vm->stack[--vm->sp] == 0;
        } else if (op == OP_JUMP_IF_FALSE_OR_POP) {
            /* and: keep a false value as the result, else drop it. */
            jump = vm->stack[vm->sp - 1] == 0;
            if (!jump) {
                vm->sp--;
            }
        } else if (op == OP_JUMP_IF_TRUE_OR_POP) {
            /* or: keep a true value as the result, else drop it. */
            jump = vm->stack[vm->sp - 1] != 0;
            if (!jump) {
                vm->sp--;
            }
        }
        if (jump) {
            pc = (uint32_t)target;
        }
        break;
    }

    case OP_PRINT:
        if (vm->sp < 1) {
            return fail(vm, "stack underflow");
        }
        print_value(vm->stack[--vm->sp]);
        break;

    case OP_HALT:
        vm->pc = (uint16_t)pc;
        vm->status = LANG_DONE;
        return LANG_DONE;

    default:
        return fail(vm, "invalid bytecode");
    }

    vm->pc = (uint16_t)pc;
    return LANG_BUDGET;
}

lang_status_t lang_vm_run(lang_vm_t *vm, uint32_t budget)
{
    vm->steps = 0;
    if (vm->status == LANG_DONE || vm->status == LANG_ERROR) {
        return vm->status;   /* finished programs stay finished */
    }
    if (vm->prog == NULL || vm->prog->len > LANG_CODE_MAX ||
        vm->prog->nvars > LANG_VARS_MAX) {
        return fail(vm, "invalid bytecode");
    }
    /* The budget check comes before each instruction, so exactly
     * `budget` instructions run, and a HALT that is the last of them
     * still finishes the program with LANG_DONE. */
    while (budget == 0 || vm->steps < budget) {
        lang_status_t s;
        vm->steps++;
        s = step(vm);
        if (s != LANG_BUDGET) {
            return s;
        }
    }
    vm->status = LANG_BUDGET;
    return LANG_BUDGET;
}
