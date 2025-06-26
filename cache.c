/*
   Work of: Aymane Bouzaidi Tiali (abouzaiditiali)
*/

#include "cache.h"
#include "pipe.h"
#include <stdio.h>

/*
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
                                                                                
cache_t* c → pointer to cache_t struct (cache)                                  
c->sets → pointer to an array of pointers to CacheLine (array of sets)          
c->sets[i] → pointer to an array of CacheLine (array of lines in a set)         
c->sets[i][j] → actual CacheLine struct (not a pointer, 1 line in a set)        
*/

cache_t* cache_new(int num_sets, int num_ways, int block_size) {                
    cache_t* c = (cache_t*)malloc(sizeof(cache_t));                             
    if (!c) return NULL;                                                        
                                                                                
    c->sets = (CacheLine**)malloc(sizeof(CacheLine*) * num_sets);               
    if (!c->sets) return NULL;                                                  
                                                                                
    for (int i = 0; i < num_sets; i++) {                                        
        c->sets[i] = (CacheLine*)malloc(sizeof(CacheLine) * num_ways);          
        if (!c->sets[i]) return NULL;                                           
                                                                                
        for (int j = 0; j < num_ways; j++) {                                    
            c->sets[i][j].valid = 0;                                            
            c->sets[i][j].rank = 0;
            c->sets[i][j].tag = 0;
            c->sets[i][j].block = (uint32_t*)malloc(block_size);
            if (!c->sets[i][j].block) return NULL;
        }
    }

    c->num_sets = num_sets;
    c->num_ways = num_ways;
    c->block_size = block_size;
    return c;
}

void cache_destroy(cache_t* c) {
    int sets = c->num_sets, ways = c->num_ways;
    for (int i = 0; i < sets; i++) {
        for (int j = 0; j < ways; j++) {
            free(c->sets[i][j].block);
        }
        free(c->sets[i]);
    }
    free(c->sets);
    free(c);
}

void block_insert(uint32_t* block, int block_size, uint64_t addr) {
    int num_ints = block_size / sizeof(uint32_t); 
    for (int i = 0; i < num_ints; i++) {
        block[i] = mem_read_32(addr + i * 4);
        printf("element at offset %d in block=%x\n", i, block[i]);
    }
}

//returns block to evict (from 0 to num_ways - 1) --- also returns LRU
int block_to_evict(cache_t* c, uint8_t index) {
    int num_ways = c->num_ways, block = 0;
    uint8_t max_rank = 0, curr_rank;
    for (int j = 0; j < num_ways; j++) {
        if (!c->sets[index][j].valid) {
            return j;
        }
        curr_rank = c->sets[index][j].rank;
        if (curr_rank > max_rank) {
            max_rank = curr_rank;
            block = j;
        }
    }
    return block;
}

void rank_update(cache_t* c, uint8_t index, int block_accessed) {
    int num_ways = c->num_ways;
    CacheLine cl_a = c->sets[index][block_accessed], cl;
    for (int j = 0; j < num_ways; j++) {
        if (j == block_accessed) { continue; }
        cl = c->sets[index][j];
        if (cl_a.valid && (cl.rank > cl_a.rank)) { continue; }
        c->sets[index][j].rank += 1;
    }
    c->sets[index][block_accessed].rank = 0;
}

uint32_t cache_read_32(cache_t* c, uint64_t addr) {                             
    int num_sets = c->num_sets, num_ways = c->num_ways;                         
    uint8_t index;                                                              
    uint64_t tag;                                                               
    if (num_sets == 64) {                                                       
        index = (addr >> 5) & 0x3F;  // bits [10:5]                                 
        tag = addr >> 11;               // bits [63:11]                            
    } else {                                                                    
        index = (addr >> 5) & 0xFF;  // bits [12:5]                                 
        tag = addr >> 13;              // bits [63:13]                             
    }                                                                           
    uint8_t block_offset = addr & 0x1F;  // bits [4:0]                                

    printf("[DEBUG][cache_read_32] addr=0x%lx, index=%u, tag=0x%lx, block_offset=%u\n",
           addr, index, tag, block_offset);

    CacheLine cl;                                                               
    for (int j = 0; j < num_ways; j++) {                                        
        cl = c->sets[index][j];                                                 
        if (cl.valid && cl.tag == tag) {                                        
            uint32_t data = cl.block[block_offset / 4];
            uint32_t expected = mem_read_32(addr);
            printf("[DEBUG][cache_read_32]   --> HIT in way %d, data=0x%x, expected=0x%x\n", j, data, expected);
            return data;                                      
        }                                                                       
    }                                                                           

    printf("[DEBUG][cache_read_32]   --> MISS\n"); 
    return -1;                                                                  
}

int cache_write_32(cache_t* c, uint64_t addr, uint32_t word) {
    int num_sets = c->num_sets, num_ways = c->num_ways;
    uint8_t index;
    uint64_t tag;
    if (num_sets == 64) {
        index = (addr >> 5) & 0x3F;
        tag = addr >> 11;
    } else {
        index = (addr >> 5) & 0xFF; 
        tag = addr >> 13;
    }
    uint8_t block_offset = addr & 0x1F;

    printf("[DEBUG][cache_write_32] addr=0x%lx, index=%u, tag=0x%lx, block_offset=%u\n",
           addr, index, tag, block_offset);

    CacheLine cl;
    for (int j = 0; j < num_ways; j++) {
        cl = c->sets[index][j];
        if (cl.valid && cl.tag == tag) {
            cl.block[block_offset / 4] = word;
            printf("[DEBUG][cache_write_32]   --> HIT in way %d, data=0x%x\n", j, cl.block[block_offset / 4]);
            return 1;
        }
    }

    printf("[DEBUG][cache_write_32]   --> MISS\n"); 
    return 0;
}

uint8_t block_hit(cache_t* c, uint8_t index, uint64_t tag) {
    int num_ways = c->num_ways;
    CacheLine cl;
    for (int j = 0; j < num_ways; j++) {
        cl = c->sets[index][j];
        if (cl.valid && cl.tag == tag) {
            return j;
        }
    }
    //should not get here
    return -1;
}

void cache_update(cache_t* c, uint64_t addr, int hit) {
    int num_sets = c->num_sets, num_ways = c->num_ways;
    uint8_t index;
    uint64_t tag;

    if (num_sets == 64) {
        index = (addr >> 5) & 0x3F;
        tag = addr >> 11;
    } else {
        index = (addr >> 5) & 0xFF;
        tag = addr >> 13;
    }

    printf("[DEBUG][cache_update] addr=0x%lx, index=%u, tag=0x%lx\n",
           addr, index, tag);

    if (hit) {
        uint8_t bh = block_hit(c, index, tag);
        printf("[DEBUG][cache_update] Cache hit — updating rank for way=%d\n", bh);
        rank_update(c, index, bh);
    } else {
        int bte = block_to_evict(c, index);
        rank_update(c, index, bte);
        printf("[DEBUG][cache_update] Cache miss - inserting block at starting addr=0x%lx into set=%u, way=%d\n",
               addr & 0xFFFFFFFFFFFFFFE0, index, bte);
        block_insert(c->sets[index][bte].block, c->block_size, addr & 0xFFFFFFFFFFFFFFE0);
        c->sets[index][bte].valid = 1;
        c->sets[index][bte].tag = tag;
    }
}

