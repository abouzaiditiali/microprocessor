#include "instruction.h"

//Assume 64-bit variant
InstructionEntry instruction_table[] = {
    {ADD,    1112, 1112, R_format},
    {ADDI,   1160, 1161, I_format},
    {ADDS,   1368, 1368, R_format},
    {ADDIS,  1416, 1417, I_format},
    {AND,    1104, 1104, R_format},
    {ANDS,   1872, 1872, R_format},
    {EOR,    1616, 1616, R_format},
    {ORR,    1360, 1360, R_format},
    {LDUR,   1986, 1986, D_format},
    {LDURB,   450,  450, D_format},
    {LDURH,   962,  962, D_format},
    {LDURW,  1474, 1474, D_format}, 
    {LS,     1690, 1691, I_format},
    {MOVZ,   1684, 1684, WI_format},
    {STUR,   1984, 1984, D_format},
    {STURW,  1472, 1472, D_format},
    {STURB,   448,  448, D_format},
    {STURH,   960,  960, D_format},
    {SUB,    1624, 1624, R_format},
    {SUBI,   1672, 1673, I_format},
    {SUBS,   1880, 1880, R_format},
    {SUBIS,  1928, 1929, I_format},
    {MUL,    1240, 1240, R_format},
    {HLT,    1698, 1698, R_format},
    {BR,     1712, 1712, R_format},
    {B,       160,  191, B_format},
    {CBNZ,   1448, 1455, CB_format},
    {CBZ,    1440, 1447, CB_format},
    {B_cond,  672,  679, CB_format}, 
};

int instruction_table_size = sizeof(instruction_table) / sizeof(InstructionEntry);

int is_load(InstructionType mnemonic) {
    return mnemonic == LDUR || mnemonic == LDURB || mnemonic == LDURH ||
        mnemonic == LDURW;
}

int is_store(InstructionType mnemonic) {
    return mnemonic == STUR || mnemonic == STURB || mnemonic == STURH ||
        mnemonic == STURW;
}

int is_branch(InstructionType mnemonic) {
    return mnemonic == BR || mnemonic == B || mnemonic == B_cond ||
        mnemonic == CBZ || mnemonic == CBNZ;
}

int does_update_flags(InstructionType mnemonic) {
    return mnemonic == ADDIS || mnemonic == SUBIS || mnemonic == ADDS || 
        mnemonic == ANDS || mnemonic == SUBS;
}

const char* stringify_mnemonic(InstructionType mnemonic) {
    switch (mnemonic) {
        case ADD:   return "ADD";
        case ADDI:  return "ADDI";
        case ADDS:  return "ADDS";
        case ADDIS: return "ADDIS";
        case AND:   return "AND";
        case ANDS:  return "ANDS";
        case EOR:   return "EOR";
        case ORR:   return "ORR";
        case LDUR:  return "LDUR";
        case LDURB: return "LDURB";
        case LDURH: return "LDURH";
        case LDURW: return "LDURW";
        case LS:    return "LS";
        case MOVZ:  return "MOVZ";
        case STUR:  return "STUR";
        case STURB: return "STURB";
        case STURH: return "STURH";
        case STURW: return "STURW";
        case SUB:   return "SUB";
        case SUBI:  return "SUBI";
        case SUBS:  return "SUBS";
        case SUBIS: return "SUBIS";
        case MUL:   return "MUL";
        case HLT:   return "HLT";
        case BR:    return "BR";
        case B:     return "B";
        case CBNZ:  return "CBNZ";
        case CBZ:   return "CBZ";
        case B_cond:return "B_cond"; 
        case NOP:   return "NOP";
        default:    return "UNKNOWN";
    }
}
