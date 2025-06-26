/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200
 */

#include "bp.h"
#include "pipe.h"
#include <stdlib.h>
#include <stdio.h>

bp_t BP;
bool pred_taken;
bool BTB_miss;
uint64_t pred_target;

void bp_predict(uint64_t PC) 
{                                                  
    uint16_t btb_index = (PC >> 2) & 0x3FF; //bits [11:2]                      
    if (BP.BTB[btb_index].valid == 0 || BP.BTB[btb_index].tag != PC) {          
        BTB_miss = true;
        CURRENT_STATE.PC += 4;                                                  
        return;                                                                 
    }                                                                           
    //at this point it is a hit                                                 
    BTB_miss = false;
    uint8_t bits = (PC >> 2) & 0xFF; //bits [9:2]                              
    uint8_t pht_index = bits ^ BP.GHR;                                          
    uint8_t counter = BP.PHT[pht_index];                                        
    if (BP.BTB[btb_index].conditional == 0 || counter >= 2) {                   
        pred_taken = true;
        pred_target = BP.BTB[btb_index].target;
        CURRENT_STATE.PC = pred_target;
        return;                                                                 
    }                                                                           
    pred_taken = false;
    CURRENT_STATE.PC += 4;
}

void bp_update(uint64_t PC, bool taken, bool conditional, uint64_t target)
{
    if (conditional) { //update PHT and GHR
        uint8_t bits = (PC >> 2) & 0xFF; //bits [9:2]                              
        uint8_t pht_index = bits ^ BP.GHR;                                          
        uint8_t counter = BP.PHT[pht_index];                                        
        if (taken) {
            if (counter != 3) {
                BP.PHT[pht_index] += 1;
            }
            BP.GHR = (BP.GHR << 1) | 0x1;
        } else {
            if (counter != 0) {
                BP.PHT[pht_index] -= 1;
            }
            BP.GHR = BP.GHR << 1;
        }
    }
    uint16_t btb_index = (PC >> 2) & 0x3FF; //bits [11:2]
    BP.BTB[btb_index].tag = PC;
    BP.BTB[btb_index].valid = 1;
    BP.BTB[btb_index].conditional = conditional;
    BP.BTB[btb_index].target = target;
}

