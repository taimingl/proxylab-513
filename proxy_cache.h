/**
 * @file proxy_cache.c
 * @brief Prototypes and definitions for proxy_cache.c
 *
 * This file contains a doubly linked list implementation to serve as
 * cache for the web server praoxy proxy.c
 *
 * @author Taiming Liu <taimingl@andrew.cmu.edu>
 */

#include "csapp.h"

#include <stddef.h> /* size_t */
#include <stdlib.h>
#include <string.h>

/* Max cache and object sizes */
#define MAX_CACHE_SIZE (1024 * 1024)
#define MAX_OBJECT_SIZE (100 * 1024)

/* Node data structure as a single cache block */
typedef struct cache_block {
    char *key;
    char *value;
    size_t block_size;
    struct cache_block *next;
    struct cache_block *prev;
} cache_block_t;

/* Data structure for the entire available cache */
typedef struct cache {
    size_t cache_size;
    cache_block_t *head;
    cache_block_t *tail;
} cache_t;

/*  */
void init_cache(cache_t *cache);

/*  */
void free_cache(cache_t *cache);

/*  */
void insert_cache(cache_t *cache, char *key, char *value, size_t buff_size);

/*  */
size_t retrieve_cache(cache_t *cache, char *search_key, char *value);
