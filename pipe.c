/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 *
 * Name: Aymane Bouzaidi Tiali
 * CNetID: abouzaiditiali
 *
 */

#include "pipe.h"
#include "shell.h"
#include "instruction.h"
#include "bp.h"
#include "cache.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

Pipe_Reg_IFtoID IFtoID;
Pipe_Reg_IDtoEX IDtoEX;
Pipe_Reg_EXtoMEM EXtoMEM;
Pipe_Reg_MEMtoWB MEMtoWB;
InstructionType stored_mnemonic;

uint8_t IF_end = 0, ID_freeze = 0, IF_freeze = 0;
uint32_t cycle_count = 0;
uint8_t ID_freeze_b = 0, IF_freeze_b = 0;
uint8_t pending_miss = 0, stall_count = 0;
uint8_t pending_miss_d = 0, stall_count_d = 0;
uint64_t stored_pc = 0, stored_addr = 0, stored_pc_d;

cache_t* i_cache;
cache_t* d_cache;

/* helpers */
int64_t sign_extend(uint32_t value_field, uint32_t num_field_bits) {         
    uint32_t shift = 64 - num_field_bits;                                       
    return ((int64_t)value_field << shift) >> shift;                            
}

void update_flags(int64_t ALUOut) {                                     
    EXtoMEM.FLAG_Z = ALUOut == 0;                    
    EXtoMEM.FLAG_N = ALUOut < 0;                     
}

int should_branch(uint32_t cond) {                                              
    switch (cond) {                                                             
        case 0x0: return IDtoEX.FLAG_Z == 1; // BEQ                      
        case 0x1: return IDtoEX.FLAG_Z == 0; // BNE                      
        case 0xC: return IDtoEX.FLAG_Z == 0 && IDtoEX.FLAG_N == 0; // BGT
        case 0xB: return IDtoEX.FLAG_N != 0; // BLT                      
        case 0xA: return IDtoEX.FLAG_N == 0; //BGE                       
        case 0xD: return !(IDtoEX.FLAG_Z == 0 && IDtoEX.FLAG_N == 0);//BLE 
        default: return 0;                                                      
    }                                                                           
}

/* global pipeline state */
CPU_State CURRENT_STATE;

void pipe_init()
{
    memset(&CURRENT_STATE, 0, sizeof(CPU_State));
    CURRENT_STATE.PC = 0x00400000;
    i_cache = cache_new(64, 4, 32);
    d_cache = cache_new(256, 8, 32);
}

void data_dependency_check() {
    if (IDtoEX.mnemonic == B_cond) {
        IDtoEX.FLAG_N = EXtoMEM.FLAG_N;
        IDtoEX.FLAG_Z = EXtoMEM.FLAG_Z;
    }
    bool forwarded1 = 0, forwarded2 = 0;
    if (!is_load(EXtoMEM.mnemonic) && EXtoMEM.mnemonic != NOP) {
        if (IDtoEX.use_reg1 && IDtoEX.reg1num == EXtoMEM.dest && EXtoMEM.reg_write) {
            if (EXtoMEM.mnemonic == MOVZ) {
                IDtoEX.op1 = EXtoMEM.imm;
            } else {
                IDtoEX.op1 = EXtoMEM.ALUOut;
            }
            forwarded1 = 1;
        }
        if (IDtoEX.use_reg2 && IDtoEX.reg2num == EXtoMEM.dest && EXtoMEM.reg_write) {
            if (EXtoMEM.mnemonic == MOVZ) {
                IDtoEX.op2 = EXtoMEM.imm;
            } else {
                IDtoEX.op2 = EXtoMEM.ALUOut;
            }
            forwarded2 = 1;
        }
    }
    if (!forwarded1 && IDtoEX.use_reg1 && IDtoEX.reg1num == MEMtoWB.dest && MEMtoWB.reg_write) {
        if (is_load(MEMtoWB.mnemonic)) {
            IDtoEX.op1 = MEMtoWB.read_data;
        } else if (MEMtoWB.mnemonic == MOVZ) {
            IDtoEX.op1 = MEMtoWB.imm;
        } else {
            IDtoEX.op1 = MEMtoWB.ALUOut;
        }
    }
    if (!forwarded2 && IDtoEX.use_reg2 && IDtoEX.reg2num == MEMtoWB.dest && MEMtoWB.reg_write) {
        if (is_load(MEMtoWB.mnemonic)) {
            IDtoEX.op2 = MEMtoWB.read_data;
        } else if (MEMtoWB.mnemonic == MOVZ) {
            IDtoEX.op2 = MEMtoWB.imm;
        } else {
            IDtoEX.op2 = MEMtoWB.ALUOut;
        }
    }
}

void pipe_cycle() {
    cycle_count++;
    printf("[DEBUG] Starting pipe_cycle() — cycle %d\n", cycle_count);

    data_dependency_check();

    pipe_stage_wb();
    pipe_stage_mem();
    pipe_stage_execute();
    pipe_stage_decode();
    pipe_stage_fetch();


    printf("[DEBUG] Ending pipe_cycle()\n\n");
}

void pipe_stage_wb() {
    if (cycle_count < 5 || MEMtoWB.mnemonic == NOP) {
        printf("[DEBUG][WB] NOP\n");
        return;
    }

    stat_inst_retire++;
    printf("[DEBUG][WB] Retiring instruction %d at PC=%lx\n", stat_inst_retire, MEMtoWB.PC);

    if (MEMtoWB.mnemonic == HLT) {
        printf("[DEBUG][WB] Halt encountered — shutting down simulator\n");
        RUN_BIT = 0;
        cache_destroy(i_cache);
        cache_destroy(d_cache);
        return;
    }

    InstructionType mnemonic = MEMtoWB.mnemonic;

    if (is_load(mnemonic)) {
        printf("[DEBUG][WB] Load result: R[%u] = 0x%lx\n", MEMtoWB.dest, MEMtoWB.read_data);
        CURRENT_STATE.REGS[MEMtoWB.dest] = MEMtoWB.read_data;

    } else if (mnemonic == MOVZ) {
        printf("[DEBUG][WB] MOVZ result: R[%u] = 0x%lx\n", MEMtoWB.dest, MEMtoWB.imm);
        CURRENT_STATE.REGS[MEMtoWB.dest] = MEMtoWB.imm;

    } else if (!is_store(mnemonic) && !is_branch(mnemonic)) {
        printf("[DEBUG][WB] ALU result: R[%u] = 0x%lx\n", MEMtoWB.dest, MEMtoWB.ALUOut);
        CURRENT_STATE.REGS[MEMtoWB.dest] = MEMtoWB.ALUOut;
    }

    if (does_update_flags(mnemonic)) {
        printf("[DEBUG][WB] Updating flags: N=%u Z=%u\n", MEMtoWB.FLAG_N, MEMtoWB.FLAG_Z);
        CURRENT_STATE.FLAG_N = MEMtoWB.FLAG_N;
        CURRENT_STATE.FLAG_Z = MEMtoWB.FLAG_Z;
    }
}

uint32_t read_update_d(uint64_t addr) {
    uint32_t res = cache_read_32(d_cache, addr);
    if (res == -1) { //cache miss
        printf("[DEBUG][MEM] Cache miss for PC=0x%lx — starting miss handling\n", CURRENT_STATE.PC);
        pending_miss_d = 1;
        stall_count_d++;
        MEMtoWB.mnemonic = NOP;
        stored_addr = addr;
        stored_pc_d = CURRENT_STATE.PC;
        if (!pending_miss) {
            bp_predict(CURRENT_STATE.PC);
        }
    } else { //cache hit
        if (stall_count_d != 11) {
            printf("[DEBUG][MEM] Cache hit for PC=0x%lx\n", CURRENT_STATE.PC);
            cache_update(d_cache, addr, 1);
        } else {
            printf("[DEBUG][MEM] Cache recovery complete — resetting stall_count\n");
            CURRENT_STATE.PC = stored_pc_d;
            stall_count_d = 0;
        }
    }
    return res;
}

int write_update_d(uint64_t addr, uint32_t word) {
    int succ = cache_write_32(d_cache, addr, word);
    if (succ == 0) { //cache miss
        printf("[DEBUG][MEM] Cache miss for addr=0x%lx — starting miss handling\n", addr);
        pending_miss_d = 1;
        stall_count_d++;
        MEMtoWB.mnemonic = NOP;
        stored_addr = addr;
        stored_pc_d = CURRENT_STATE.PC;
        if (!pending_miss) {
            bp_predict(CURRENT_STATE.PC);
        }
    } else { //cache hit
        if (stall_count_d != 11) {
            printf("[DEBUG][MEM] Cache hit for addr=0x%lx\n", addr);
            cache_update(d_cache, addr, 1);
            mem_write_32(addr, word);
        } else {
            printf("[DEBUG][MEM] Cache recovery complete — resetting stall_count\n");
            CURRENT_STATE.PC = stored_pc_d;
            stall_count_d = 0;
            mem_write_32(addr, word);
        }
    }
    return succ;
}

void pipe_stage_mem() {
    if (EXtoMEM.mnemonic == NOP) {
        printf("[DEBUG][MEM] NOP\n");
        MEMtoWB.mnemonic = EXtoMEM.mnemonic;
        return;
    }

    printf("[DEBUG][MEM] Starting MEM stage for instruction at PC=%lx\n", EXtoMEM.PC);

    if (pending_miss_d) {
        stall_count_d++;
        printf("[DEBUG][MEM] Data miss stall — cycle %d of 10, addr=0x%lx\n", stall_count_d, stored_addr);
        if (stall_count_d == 10) {
            printf("[DEBUG][MEM] Updating D-cache at 0x%lx\n", stored_addr);
            cache_update(d_cache, stored_addr, 0);
            pending_miss_d = 0;
            stall_count_d++;
        }
        MEMtoWB.mnemonic = NOP;
        return;
    }

    switch (EXtoMEM.mnemonic) {
        case LDUR:
        {
            printf("[DEBUG][MEM] LDUR — reading 64 bits from 0x%lx\n", EXtoMEM.ALUOut);
            uint32_t low = read_update_d(EXtoMEM.ALUOut);
            if (low == -1) { printf("[DEBUG][MEM] LDUR low read miss\n"); return; }
            uint32_t high = read_update_d(EXtoMEM.ALUOut + 4);
            if (high == -1) { printf("[DEBUG][MEM] LDUR high read miss\n"); return; }
            MEMtoWB.read_data = ((uint64_t)high << 32) | low;
            break;
        }
        case LDURB:
        {
            printf("[DEBUG][MEM] LDURB — reading byte from 0x%lx\n", EXtoMEM.ALUOut);
            uint32_t word = read_update_d(EXtoMEM.ALUOut);
            if (word == -1) { return; }
            MEMtoWB.read_data = word & 0xFF;
            break;
        }
        case LDURH:
        {
            printf("[DEBUG][MEM] LDURH — reading halfword from 0x%lx\n", EXtoMEM.ALUOut);
            uint32_t word = read_update_d(EXtoMEM.ALUOut);
            if (word == -1) { return; }
            MEMtoWB.read_data = word & 0xFFFF;
            break;
        }
        case LDURW:
        {
            printf("[DEBUG][MEM] LDURW — reading word from 0x%lx\n", EXtoMEM.ALUOut);
            uint32_t word = read_update_d(EXtoMEM.ALUOut);
            if (word == -1) { return; }
            MEMtoWB.read_data = word;
            break;
        }
        case STUR:
        {
            printf("[DEBUG][MEM] STUR — writing 64 bits to 0x%lx\n", EXtoMEM.ALUOut);
            int succ1 = write_update_d(EXtoMEM.ALUOut, (uint32_t)EXtoMEM.write_data & 0xFFFFFFFF);
            if (!succ1) { return; }
            int succ2 = write_update_d(EXtoMEM.ALUOut + 4, (uint64_t)EXtoMEM.write_data >> 32);
            if (!succ2) { return; }
            break;
        }
        case STURB:
        {
            printf("[DEBUG][MEM] STURB — writing byte to 0x%lx\n", EXtoMEM.ALUOut);
            uint8_t value = (uint8_t)EXtoMEM.write_data & 0xFF;
            uint32_t old32 = read_update_d(EXtoMEM.ALUOut);
            if (old32 == -1) { return; }
            old32 = (old32 & 0xFFFFFF00) | value;
            int succ = write_update_d(EXtoMEM.ALUOut, old32);
            if (!succ) { return; }
            break;
        }
        case STURH:
        {
            printf("[DEBUG][MEM] STURH — writing halfword to 0x%lx\n", EXtoMEM.ALUOut);
            uint16_t value = (uint16_t)EXtoMEM.write_data & 0xFFFF;
            uint32_t old32 = read_update_d(EXtoMEM.ALUOut);
            if (old32 == -1) { return; }
            old32 = (old32 & 0xFFFF0000) | value;
            int succ = write_update_d(EXtoMEM.ALUOut, old32);
            if (!succ) { return; }
            break;
        }
        case STURW:
        {
            printf("[DEBUG][MEM] STURW — writing word to 0x%lx\n", EXtoMEM.ALUOut);
            uint32_t value = (uint32_t)EXtoMEM.write_data & 0xFFFFFFFF;
            int succ = write_update_d(EXtoMEM.ALUOut, value);
            if (!succ) { return; }
            break;
        }
    }

    MEMtoWB.ALUOut = EXtoMEM.ALUOut;
    MEMtoWB.imm = EXtoMEM.imm;
    MEMtoWB.dest = EXtoMEM.dest;
    MEMtoWB.FLAG_N = EXtoMEM.FLAG_N;
    MEMtoWB.FLAG_Z = EXtoMEM.FLAG_Z;
    MEMtoWB.mnemonic = EXtoMEM.mnemonic;
    MEMtoWB.reg1num = EXtoMEM.reg1num;
    MEMtoWB.reg2num = EXtoMEM.reg2num;
    MEMtoWB.use_reg1 = EXtoMEM.use_reg1;
    MEMtoWB.use_reg2 = EXtoMEM.use_reg2;
    MEMtoWB.reg_write = EXtoMEM.reg_write;
    MEMtoWB.PC = EXtoMEM.PC;
}


bool maybe_flush(uint64_t act_target, bool act_taken, bool conditional) {
    if (IDtoEX.BTB_miss && !(conditional && !act_taken)) {
        return true;
    }
    if (IDtoEX.pred_taken && (IDtoEX.pred_target != act_target)) {
        return true;
    }
    if (IDtoEX.pred_taken != act_taken) {
        return true;
    }
    return false;
}

void pipe_stage_execute()
{
    if (pending_miss_d || stall_count_d == 11) {
        printf("[DEBUG][EX] Skipping due to pending data miss or stall (stall_count_d = %d)\n", stall_count_d);
        return;
    }

    if (IDtoEX.mnemonic == NOP) {
        printf("[DEBUG][EX] NOP\n");
        EXtoMEM.mnemonic = IDtoEX.mnemonic;
        return;
    }

    printf("[DEBUG][EX] Executing instruction at PC=0x%lx\n", IDtoEX.PC);

    //control dependency handling 
    switch (IDtoEX.mnemonic) {                                                         
        case BR:                                                                
        {                                                                       
            if (IF_end) {
                break;
            }
            uint64_t nPC = (uint64_t)IDtoEX.op1;
            if (maybe_flush(nPC, 1, 0)) {
                ID_freeze_b = 1;
                IF_freeze_b = 1;
                CURRENT_STATE.PC = nPC;
            }
            bp_update(IDtoEX.PC, 1, 0, nPC);
            break;                                                             
        }                                                                       
        case B:                                                                 
        {                                                                      
            if (IF_end) {
                break;
            }
            uint64_t nPC = IDtoEX.PC + (uint64_t)(IDtoEX.offset << 2);
            if (maybe_flush(nPC, 1, 0)) {
                ID_freeze_b = 1;
                IF_freeze_b = 1;
                CURRENT_STATE.PC = nPC;
            }
            bp_update(IDtoEX.PC, 1, 0, nPC);
            break;                                                             
        }                                                                       
        case CBZ:                                                               
        {                                                                       
            if (IF_end) {
                break;
            }
 
            bool taken = (IDtoEX.op1 == 0);
            uint64_t target = IDtoEX.PC + (uint64_t)(IDtoEX.offset << 2);
            uint64_t next = IDtoEX.PC + 4;
            if (maybe_flush(target, taken, 1)) {
                ID_freeze_b = 1;
                IF_freeze_b = 1;
                if (taken) {
                    CURRENT_STATE.PC = target;
                } else {
                    CURRENT_STATE.PC = next;
                }
            }
            if (taken) {
                bp_update(IDtoEX.PC, 1, 1, target);
            } else {
                bp_update(IDtoEX.PC, 0, 1, next);
            }
            break;                                                              
        }                                                                       
        case CBNZ:                                                              
        {                                                                       
            if (IF_end) {
                break;
            }
 
            bool taken = (IDtoEX.op1 != 0);
            uint64_t target = IDtoEX.PC + (uint64_t)(IDtoEX.offset << 2);
            uint64_t next = IDtoEX.PC + 4;
            if (maybe_flush(target, taken, 1)) {
                ID_freeze_b = 1;
                IF_freeze_b = 1;
                if (taken) {
                    CURRENT_STATE.PC = target;
                } else {
                    CURRENT_STATE.PC = next;
                }
            }
            if (taken) {
                bp_update(IDtoEX.PC, 1, 1, target);
            } else {
                bp_update(IDtoEX.PC, 0, 1, next);
            }
            break;
        }                                                                       
        case B_cond:                                                            
        {                                                                       
            if (IF_end) {
                break;
            }
 
            bool taken = should_branch(IDtoEX.reg1num);
            uint64_t target = IDtoEX.PC + (uint64_t)(IDtoEX.offset << 2);
            uint64_t next = IDtoEX.PC + 4;
            if (maybe_flush(target, taken, 1)) {
                ID_freeze_b = 1;
                IF_freeze_b = 1;
                if (taken) {
                    CURRENT_STATE.PC = target;
                } else {
                    CURRENT_STATE.PC = next;
                }
            }
            if (taken) {
                bp_update(IDtoEX.PC, 1, 1, target);
            } else {
                bp_update(IDtoEX.PC, 0, 1, next);
            }
            break;
        }                                                                       
 
        case HLT:
        {
            IF_end = 1;
            if (!pending_miss) {
                CURRENT_STATE.PC = IDtoEX.PC + 8;
            }
            EXtoMEM.mnemonic = IDtoEX.mnemonic;
            return;
        }
        case ADD:                                                               
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 + IDtoEX.op2;
            break;                                                              
        }                                                                       
        case ADDS:                                                              
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 + IDtoEX.op2;
            update_flags(EXtoMEM.ALUOut);                                          
            break;                                                              
        }                                                                       
        case AND:                                                               
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 & IDtoEX.op2;
            break;                                                              
        }                                                                       
        case ANDS:                                                              
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 & IDtoEX.op2;
            update_flags(EXtoMEM.ALUOut);                                          
            break;                                                              
        }                                                                       
        case EOR:                                                               
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 ^ IDtoEX.op2;
            break;                                                              
        }                                                                       
        case ORR:                                                               
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 | IDtoEX.op2;
            break;                                                              
        }                                                                       
        case SUB:                                                               
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 - IDtoEX.op2;
            break;                                                              
        }                                                                       
        case SUBS: //Like CMP                                                   
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 - IDtoEX.op2;
            update_flags(EXtoMEM.ALUOut);                                          
            if (IDtoEX.dest == 31) {                                            
                 EXtoMEM.ALUOut = 0;
            }                                                                   
            break;                                                              
        }                                                                       
        case MUL:                                                               
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 * IDtoEX.op2;
            break;                                                              
        }                                                                       
        //transition into I-format                                              
        case ADDI:                                                              
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.imm + IDtoEX.op1;
            break;                                                              
        }                                                                       
        case ADDIS:                                                             
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.imm + IDtoEX.op1;
            update_flags(EXtoMEM.ALUOut);                                          
            break;                                                              
        }                                                                       
        case LS:                                                                
        {                                                                       
            if ((IDtoEX.imm & 0x3f) == 0x3f) { //LSR                        
                uint32_t shamt = (IDtoEX.imm >> 6) & 0x3f;                  
                uint64_t to_shift = (uint64_t)IDtoEX.op1;
                EXtoMEM.ALUOut = to_shift >> shamt;               
            } else { //LSL                                                      
                EXtoMEM.ALUOut = IDtoEX.op1 << ((IDtoEX.imm & 0x3f) ^ 0x3f);
            }                                                                   
            break;                                                              
        }                                                                       
        case SUBI:                                                              
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 - IDtoEX.imm;
            break;                                                              
        }                                                                       
        case SUBIS: //Like CMPI                                                 
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 - IDtoEX.imm;
            update_flags(EXtoMEM.ALUOut);                                          
            if (IDtoEX.dest == 31) {                                            
                 EXtoMEM.ALUOut = 0;
            }                                                                   
            break;                                                              
        }                                                                       
        //transition into D-format                                              
        case LDUR:                                                              
        case LDURB:                                                             
        case LDURH:                                                             
        case LDURW:                                                             
        case STUR:
        case STURB:
        case STURH:
        case STURW:
        {                                                                       
            EXtoMEM.ALUOut = IDtoEX.op1 + IDtoEX.offset;
            break;                                                              
        }                                                                       
    }                                                                           
    if ((pending_miss && is_branch(IDtoEX.mnemonic)) &&
                        ((CURRENT_STATE.PC >> 5) != (stored_pc >> 5))) {
        printf("[DEBUG][EX] Flushing pending branch miss\n");
        pending_miss = 0;
        stall_count = 0;
    }
    if (pending_miss) {
        IF_freeze_b = 0;
        ID_freeze_b = 0;
    }
    EXtoMEM.mnemonic = IDtoEX.mnemonic;
    EXtoMEM.dest = IDtoEX.dest;
    EXtoMEM.write_data = IDtoEX.op2;
    EXtoMEM.imm = IDtoEX.imm;
    EXtoMEM.reg1num = IDtoEX.reg1num;
    EXtoMEM.reg2num = IDtoEX.reg2num;
    EXtoMEM.use_reg1 = IDtoEX.use_reg1;
    EXtoMEM.use_reg2 = IDtoEX.use_reg2;
    EXtoMEM.reg_write = IDtoEX.reg_write;
    EXtoMEM.PC = IDtoEX.PC;

}

void pipe_stage_decode()
{
    if (pending_miss_d || stall_count_d == 11) {
        printf("[DEBUG][ID] Skipping due to data stall or pending miss (stall_count_d = %d)\n", stall_count_d);
        return;
    }

    if (ID_freeze_b) {
        printf("[DEBUG][ID] ID freeze_b triggered — injecting NOP\n");
        ID_freeze_b = 0;
        IDtoEX.mnemonic = NOP;
        return;
    }
    if (ID_freeze) {
        printf("[DEBUG][ID] Load-use stall recovery — restoring mnemonic %d\n", stored_mnemonic);
        IDtoEX.mnemonic = stored_mnemonic;
        ID_freeze = 0;
        IF_freeze = 1;
        return;
    }
    if (IFtoID.mnemonic == NOP) {
        printf("[DEBUG][ID] NOP\n");
        IDtoEX.mnemonic = NOP;
        return;
    }
 
    IDtoEX.pred_taken = IFtoID.pred_taken;
    IDtoEX.BTB_miss = IFtoID.BTB_miss;
    IDtoEX.pred_target = IFtoID.pred_target;

    IDtoEX.PC = IFtoID.PC;
    IDtoEX.mnemonic = IFtoID.mnemonic;
    uint32_t opcode11 = IFtoID.instruction >> 21;                                      
    InstructionEntry ie;                                                        
    uint32_t instruction = IFtoID.instruction;

    printf("[DEBUG][ID] Decoding instruction at PC=0x%lx\n", IFtoID.PC);

    for (unsigned int i = 0; i < instruction_table_size; i++) {                              
        ie = instruction_table[i];                                              
        if (ie.opcode_start <= opcode11 && opcode11 <= ie.opcode_end) {         
            IDtoEX.mnemonic = ie.mnemonic;                                             
            switch (ie.format) {                                                
                case R_format:                                                  
                {                                                               
                    IDtoEX.reg1num = (instruction >> 5) & 0x1F;
                    IDtoEX.reg2num = (instruction >> 16) & 0x1F;
                    if (!is_branch(ie.mnemonic)) {
                        IDtoEX.use_reg1 = 1;
                        IDtoEX.use_reg2 = 1;
                        IDtoEX.reg_write = 1;
                    } else {
                        IDtoEX.use_reg1 = 0;
                        IDtoEX.use_reg2 = 0;
                        IDtoEX.reg_write = 0;
                    }
                    IDtoEX.op1 = CURRENT_STATE.REGS[IDtoEX.reg1num];
                    IDtoEX.op2 = CURRENT_STATE.REGS[IDtoEX.reg2num];
                    IDtoEX.dest = instruction & 0x1F;
                    break;
                }                                                               
                case I_format:                                                  
                {                                                               
                    IDtoEX.reg1num = (instruction >> 5) & 0x1F;
                    IDtoEX.op1 = CURRENT_STATE.REGS[IDtoEX.reg1num];
                    IDtoEX.use_reg1 = 1;
                    IDtoEX.use_reg2 = 0;
                    IDtoEX.reg_write = 1;
                    IDtoEX.imm = (instruction >> 10) & 0xFFF;
                    IDtoEX.dest = instruction & 0x1F;
                    break;
                }                                                               
                case B_format:                                                  
                {                                                               
                    IDtoEX.offset = sign_extend(instruction & 0x3FFFFFF, 26);                   
                    IDtoEX.reg_write = 0;
                    return;
                }                                                               
                case CB_format:                                                 
                {                                                               
                    IDtoEX.offset = sign_extend((instruction >> 5) & 0x7FFFF, 19);             
                    IDtoEX.reg1num = instruction & 0x1F;
                    IDtoEX.op1 = CURRENT_STATE.REGS[IDtoEX.reg1num];
                    IDtoEX.use_reg1 = 1;
                    IDtoEX.use_reg2 = 0;
                    IDtoEX.reg_write = 0;
                    return;
                }                                                               
                case WI_format:                                                 
                {                                                               
                    IDtoEX.imm = (instruction >> 5) & 0xFFFF;
                    IDtoEX.dest = instruction & 0x1F;
                    IDtoEX.use_reg1 = 0;
                    IDtoEX.use_reg2 = 0;
                    IDtoEX.reg_write = 1;
                    return;
                }                                                               
                case D_format:                                                  
                {                                                               
                    IDtoEX.reg1num = (instruction >> 5) & 0x1F;
                    IDtoEX.use_reg1 = 1;
                    IDtoEX.op1 = CURRENT_STATE.REGS[IDtoEX.reg1num];
                    IDtoEX.offset = sign_extend((instruction >> 12) & 0x1FF, 9);                
                    if (is_store(ie.mnemonic)) {
                        IDtoEX.reg2num = instruction & 0x1F;
                        IDtoEX.use_reg2 = 1;
                        IDtoEX.reg_write = 0;
                        IDtoEX.op2 = CURRENT_STATE.REGS[IDtoEX.reg2num];
                    } else {
                        IDtoEX.dest = instruction & 0x1F;                           
                        IDtoEX.reg_write = 1;
                        IDtoEX.use_reg2 = 0;
                    }
                    break;
                }                                                               
            }                                                                   
        }                                                                       
    }                                                                           
    //load dependency check
    if (is_load(EXtoMEM.mnemonic) && 
            ((EXtoMEM.dest == IDtoEX.reg1num && IDtoEX.use_reg1) ||
                (EXtoMEM.dest == IDtoEX.reg2num && IDtoEX.use_reg2))) {
        stored_mnemonic = IDtoEX.mnemonic;
        IDtoEX.mnemonic = NOP;
        ID_freeze = 1;
    }
}

void pipe_stage_fetch() {
    if ((!pending_miss && pending_miss_d) || stall_count_d == 11) {
        printf("[DEBUG][IF] Skipping fetch — data stall or miss (stall_count_d = %d)\n", stall_count_d);
        return;
    }

    if (IF_end) {
        printf("[DEBUG][IF] Fetch ended — IF_end set\n");
        return;
    }

    if (IF_freeze_b) {
        printf("[DEBUG][IF] IF freeze_b triggered — injecting NOP\n");
        IFtoID.mnemonic = NOP;
        IF_freeze_b = 0;
        return;
    }

    if (IF_freeze) {
        printf("[DEBUG][IF] IF freeze triggered — stalling PC=0x%lx\n", CURRENT_STATE.PC);
        IF_freeze = 0;
        return;
    }

    if (pending_miss) {
        stall_count++;
        printf("[DEBUG][IF] Instruction miss stall — cycle %d of 10, PC=0x%lx\n", stall_count, CURRENT_STATE.PC);
        if (stall_count == 10) {
            printf("[DEBUG][IF] Updating I-cache at PC=0x%lx\n", CURRENT_STATE.PC);
            cache_update(i_cache, CURRENT_STATE.PC, 0);
            pending_miss = 0;
            stall_count++;
        }
        IFtoID.mnemonic = NOP;
        return;
    }

    uint32_t res = cache_read_32(i_cache, CURRENT_STATE.PC);
    if (res == -1) {
        printf("[DEBUG][IF] Cache miss for PC=0x%lx — starting miss handling\n", CURRENT_STATE.PC);
        pending_miss = 1;
        stall_count++;
        IFtoID.mnemonic = NOP;
        stored_pc = CURRENT_STATE.PC;
        return;
    } else {
        if (stall_count != 11) {
            printf("[DEBUG][IF] Cache hit for PC=0x%lx\n", CURRENT_STATE.PC);
            cache_update(i_cache, CURRENT_STATE.PC, 1);
        } else {
            printf("[DEBUG][IF] Cache recovery complete — resetting stall_count\n");
            stall_count = 0;
        }
    }
    IFtoID.instruction = res;

    // Update IF/ID pipeline register
    IFtoID.PC = CURRENT_STATE.PC;
    IFtoID.mnemonic = UNKNOWN;

    // Perform branch prediction
    bp_predict(CURRENT_STATE.PC);
    IFtoID.pred_taken = pred_taken;
    IFtoID.BTB_miss = BTB_miss;
    IFtoID.pred_target = pred_target;

    //printf("[DEBUG][IF] Branch prediction: pred_taken=%d, BTB_miss=%d, target=0x%lx\n",
    //       pred_taken, BTB_miss, pred_target);
}

