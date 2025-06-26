#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <inttypes.h>

//Intruction Formats
typedef enum { R_format, I_format, D_format, B_format, CB_format, WI_format
} InstructionFormat;

//Supported Instruction Types
typedef enum { 
    ADD, ADDI, ADDS, ADDIS,
    AND, ANDS,
    EOR,
    ORR,
    LDUR, LDURB, LDURH, LDURW, LS,
    MOVZ,
    STUR, STURB, STURH, STURW, 
    SUB, SUBI, SUBS, SUBIS,
    MUL,
    HLT,
    BR, B,
    CBNZ, CBZ,
    B_cond, BEQ, BNE, BGT, BLT, BGE, BLE,
    NOP, UNKNOWN
} InstructionType;

typedef struct {
    InstructionType mnemonic;
    uint32_t opcode_start, opcode_end;
    InstructionFormat format;
} InstructionEntry;

extern InstructionEntry instruction_table[];
extern int instruction_table_size;

int is_load(InstructionType mnemonic);
int is_store(InstructionType mnemonic);
int is_branch(InstructionType mnemonic);
int does_update_flags(InstructionType mnemonic);
const char* stringify_mnemonic(InstructionType mnemonic);

#endif // INSTRUCTION_H
