/*
 * CMSC 22200, Fall 2016
 *
 * ARM pipeline timing simulator
 *
 */
#ifndef _CACHE_H_
#define _CACHE_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    uint8_t valid, rank;
    uint64_t tag;
    uint32_t* block;
} CacheLine;

typedef struct
{
    CacheLine** sets;
    int num_sets, num_ways, block_size;
} cache_t;

/*
cache_t* c → pointer to cache_t struct (cache)
c->sets → pointer to an array of pointers to CacheLine (array of sets)
c->sets[i] → pointer to an array of CacheLine (array of lines in a set)
c->sets[i][j] → actual CacheLine struct (not a pointer, 1 line in a set)
*/

cache_t* cache_new(int num_sets, int num_ways, int block_size);
                                                                                
void cache_destroy(cache_t* c); 
                                                                                
void cache_update(cache_t* c, uint64_t addr, int hit);
                                                                                
uint32_t cache_read_32(cache_t* c, uint64_t addr);
int cache_write_32(cache_t* c, uint64_t addr, uint32_t word);

#endif 
