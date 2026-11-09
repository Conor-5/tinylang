/*
 * opcodes.h - bytecode opcodes (bytecode format version 1).
 *
 * See docs/bytecode.md for operands and stack effects. Values are part of
 * the bytecode format: never renumber, only add (and bump the version).
 * 0 is deliberately not an opcode, so zeroed memory is invalid bytecode.
 */
#ifndef OPCODES_H
#define OPCODES_H

enum {
    OP_PUSH_INT = 0x01,           /* i32 operand */
    OP_LOAD = 0x02,               /* u8 operand: variable slot */
    OP_STORE = 0x03,              /* u8 operand: variable slot */
    OP_POP = 0x04,
    OP_ADD = 0x05,
    OP_SUB = 0x06,
    OP_MUL = 0x07,
    OP_FLOORDIV = 0x08,
    OP_MOD = 0x09,
    OP_NEG = 0x0A,
    OP_NOT = 0x0B,
    OP_EQ = 0x0C,
    OP_NE = 0x0D,
    OP_LT = 0x0E,
    OP_GT = 0x0F,
    OP_LE = 0x10,
    OP_GE = 0x11,
    OP_JUMP = 0x12,               /* i16 operand */
    OP_JUMP_IF_FALSE = 0x13,      /* i16 operand */
    OP_JUMP_IF_FALSE_OR_POP = 0x14, /* i16 operand */
    OP_JUMP_IF_TRUE_OR_POP = 0x15,  /* i16 operand */
    OP_PRINT = 0x16,
    OP_HALT = 0x17
};

#endif /* OPCODES_H */
