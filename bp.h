/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200
 */

#ifndef _BP_H_
#define _BP_H_

#include <inttypes.h>
#include <stdbool.h>

typedef struct {
    uint64_t tag;
    bool valid;
    bool conditional;
    uint64_t target;
} BTBEntry;

typedef struct {
    uint8_t GHR;
    uint8_t PHT[256]; //not efficient but practical, only need two bits
    BTBEntry BTB[1024];
} bp_t;

extern bp_t BP;
extern bool pred_taken;
extern bool BTB_miss;
extern uint64_t pred_target;

void bp_predict(uint64_t PC);
void bp_update(uint64_t PC, bool taken, bool conditional, uint64_t target);

#endif
