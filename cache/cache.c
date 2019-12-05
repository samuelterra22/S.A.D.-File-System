//
// Created by arthur on 05/12/2019.
//

#include "cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int search_next_free_input(cache_entry_t* cache) {
    for(int i = 0; i < CACHE_MAX_SIZE; i++) {
        if (cache[i].is_valid == FALSE)
            return i;
    }

    return -1;
}

int cache_search_entry_list_index(cache_entry_t* cache, char* path) {
    for(int i = 0; i < CACHE_MAX_SIZE; i++) {
        if (cache[i].is_valid == TRUE && strcmp(path, cache[i].path) == 0 && cache[i].type == CACHE_ENTRY_LIST) {
            return i;
        }
    }
    return -1;
}

int cache_search_entry_index(cache_entry_t* cache, char* path) {
    for(int i = 0; i < CACHE_MAX_SIZE; i++) {
        if (cache[i].is_valid == TRUE && strcmp(path, cache[i].path) == 0 && cache[i].type == CACHE_ENTRY) {
            return i;
        }
    }
    return -1;
}

void init_cache(cache_entry_t* cache) {
    for(int i = 0; i < CACHE_MAX_SIZE; i++) {
        cache[i].is_valid = FALSE;
    }
}

int cache_add_entry(cache_entry_t* cache, char* path, dir_entry_t* entry) {
    int get_next_entry = search_next_free_input(cache);
    if (get_next_entry == -1) {
        get_next_entry = rand() % CACHE_MAX_SIZE;
    }

    strcpy(cache[get_next_entry].path, path);
    cache[get_next_entry].type = CACHE_ENTRY;
    cache[get_next_entry].is_valid = TRUE;
    memcpy(&cache[get_next_entry].data.dir_entry, entry, sizeof(dir_entry_t));

    return 1;
}

int cache_add_entry_list(cache_entry_t* cache, char* path, dir_entry_t* entry_list) {
    int get_next_entry = search_next_free_input(cache);
    if (get_next_entry == -1) {
        get_next_entry = rand() % CACHE_MAX_SIZE;
    }

    strcpy(cache[get_next_entry].path, path);
    cache[get_next_entry].type = CACHE_ENTRY_LIST;
    cache[get_next_entry].is_valid = TRUE;
    memcpy(&cache[get_next_entry].data.dir_entry_list, entry_list, SECTOR_SIZE);

    fprintf(stderr, "cache - Adicionou dado na posicao %d\n", get_next_entry);

    return 1;
}

int cache_search_entry_list(cache_entry_t* cache, char* path, dir_entry_t** actual_dir_entry) {
    for(int i = 0; i < CACHE_MAX_SIZE; i++) {
        if (cache[i].is_valid == TRUE && strcmp(path, cache[i].path) == 0 && cache[i].type == CACHE_ENTRY_LIST) {
            memcpy(*actual_dir_entry, cache[i].data.dir_entry_list, SECTOR_SIZE);
            fprintf(stderr, "cache - Achou a entrada no indice %d\n", i);
            return 1;
        }
    }

    fprintf(stderr, "cache - Nao encontrou entrada\n");
    return -1;
}

int cache_search_entry(cache_entry_t* cache, char* path, dir_entry_t* actual_dir_entry) {
    for(int i = 0; i < CACHE_MAX_SIZE; i++) {
        if (cache[i].is_valid == TRUE && strcmp(path, cache[i].path) == 0 && cache[i].type == CACHE_ENTRY) {
            memcpy(actual_dir_entry, &cache[i].data.dir_entry, sizeof(dir_entry_t));
            fprintf(stderr, "cache - Achou a entrada no indice %d\n", i);
            return 1;
        }
    }

    fprintf(stderr, "cache - Nao encontrou entrada\n");
    return -1;
}

int cache_update_entry_list(cache_entry_t* cache, char* path, dir_entry_t* entry_list) {
    int index_in_cache = cache_search_entry_list_index(cache, path);
    if (index_in_cache == -1) return -1;

    strcpy(cache[index_in_cache].path, path);
    cache[index_in_cache].type = CACHE_ENTRY_LIST;
    cache[index_in_cache].is_valid = TRUE;
    memcpy(&cache[index_in_cache].data.dir_entry_list, entry_list, SECTOR_SIZE);

    return 1;
}

int cache_update_entry(cache_entry_t* cache, char* path, dir_entry_t* entry) {
    int index_in_cache = cache_search_entry_index(cache, path);
    if (index_in_cache == -1) return -1;

    strcpy(cache[index_in_cache].path, path);
    cache[index_in_cache].type = CACHE_ENTRY;
    cache[index_in_cache].is_valid = TRUE;
    memcpy(&cache[index_in_cache].data.dir_entry, entry, sizeof(dir_entry_t));

    return 1;
}
