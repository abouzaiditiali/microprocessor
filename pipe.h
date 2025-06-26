/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */

#ifndef _PIPE_H_
#define _PIPE_H_

#include "shell.h"
#include "instruction.h"
#include "stdbool.h"
#include <limits.h>


typedef struct CPU_State {
	/* register file state */
	int64_t REGS[ARM_REGS];
	int FLAG_N;        /* flag N */
	int FLAG_Z;        /* flag Z */

	/* program counter in fetch stage */
	uint64_t PC;
} CPU_State;

typedef struct Pipe_Reg_IFtoID {
    uint64_t PC;
    uint32_t instruction;
    InstructionType mnemonic;
    bool pred_taken, BTB_miss;
    uint64_t pred_target;
} Pipe_Reg_IFtoID;

typedef struct Pipe_Reg_IDtoEX {
    InstructionType mnemonic;
    uint64_t PC, imm;
    int64_t op1, op2, offset;
    uint32_t dest, rt, reg1num, reg2num;
    uint8_t FLAG_N, FLAG_Z, use_reg1, use_reg2, reg_write;
    bool pred_taken, BTB_miss;
    uint64_t pred_target;
} Pipe_Reg_IDtoEX;

typedef struct Pipe_Reg_EXtoMEM {
    InstructionType mnemonic;
    uint64_t nPC, write_data, imm;
    int64_t ALUOut, offset, PC; 
    uint32_t dest, rt, reg1num, reg2num;
    uint8_t FLAG_N, FLAG_Z, use_reg1, use_reg2, reg_write; 
} Pipe_Reg_EXtoMEM;

typedef struct Pipe_Reg_MEMtoWB {
    InstructionType mnemonic;
    int64_t ALUOut;
    uint32_t dest, reg1num, reg2num;
    uint64_t read_data, imm, PC;
    uint8_t FLAG_N, FLAG_Z, use_reg1, use_reg2, reg_write;
} Pipe_Reg_MEMtoWB;

int RUN_BIT;

/* global variable -- pipeline state */
extern CPU_State CURRENT_STATE;

/* called during simulator startup */
void pipe_init();

/* this function calls the others */
void pipe_cycle();

/* each of these functions implements one stage of the pipeline */
void pipe_stage_fetch();
void pipe_stage_decode();
void pipe_stage_execute();
void pipe_stage_mem();
void pipe_stage_wb();

#endif
