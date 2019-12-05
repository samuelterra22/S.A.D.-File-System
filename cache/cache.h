//
// Created by arthur on 05/12/2019.
//

#ifndef SAD_CACHE_H
#define SAD_CACHE_H

#include "../minifat/minifat.h"
#include <stdint.h>

typedef uint8_t bool;
#define TRUE 1
#define FALSE 0

#define CACHE_ENTRY 0
#define CACHE_ENTRY_LIST 1
#define CACHE_MAX_SIZE 1024

typedef union {
    dir_entry_t dir_entry;
    char* dir_entry_list[SECTOR_SIZE];
} cache_data_t;

struct cache_entry {
    char path[MAXPATHLENGTH];
    bool is_valid;
    uint8_t type;
    cache_data_t data;
} __attribute__((packed));

typedef struct cache_entry cache_entry_t;

void init_cache(cache_entry_t* cache);
int cache_add_entry(cache_entry_t* cache, char* path, dir_entry_t* entry);
int cache_add_entry_list(cache_entry_t* cache, char* path, dir_entry_t* entry_list);
int cache_search_entry_list(cache_entry_t* cache, char* path, dir_entry_t** entry_list);
int cache_search_entry(cache_entry_t* cache, char* path, dir_entry_t* actual_dir_entry);

#endif //SAD_CACHE_H
