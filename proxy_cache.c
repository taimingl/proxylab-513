/**
 * @file proxy_cache.c
 * @brief functions for the proxy server
 *
 * @author Taiming Liu <taimingl@andrew.cmu.edu>
 */
#include "proxy_cache.h"
#include "csapp.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* Global variables */
// cache_block_t *head;
// cache_block_t *tail;
// size_t cache_size;

/* Global lock to synchronize shared resource */
static pthread_mutex_t mutex;

/**
 * @brief Initializes cache structure for web server to use.
 * @param[in] cache pointer to the cache struct to be initialized.
 *
 */
void init_cache(cache_t *cache) {
    cache = (cache_t *)Malloc(sizeof(cache_t));
    cache->cache_size = 0;
    cache->head = NULL;
    cache->tail = NULL;
    // cache->head = (cache_block_t *)Malloc(sizeof(cache_block_t));
    // cache->tail = (cache_block_t *)Malloc(sizeof(cache_block_t));
    // cache->head->next = NULL;
    // cache->head->prev = NULL;
    // cache->tail->next = NULL;
    // cache->tail->prev = NULL;
    // cache->head = cache->tail;
    pthread_mutex_init(&mutex, NULL);
}

/**
 * @brief Private helper function to remove one cache block from cache.
 * @param[in] cache pointer to the cache.
 * @param[in] cache_block cache block to be removed
 *
 */
static void evict_one_cb(cache_t *cache, cache_block_t *curr_cb) {
    // Note: all calling functions only intend to remove from the tail
    // if (curr_cb == cache->tail) {
    //     cache->tail = curr_cb->prev;
    // }
    // curr_cb->next->prev = curr_cb->prev;
    // curr_cb->prev->next = curr_cb->next;
    // cache->cache_size -= curr_cb->block_size;
    // // cache->num_block--;
    // curr_cb->prev = NULL;
    // curr_cb->next = NULL;
    // // Free(curr_cb->key);
    // Free(curr_cb->value);
    // Free(curr_cb);

    cache->cache_size -= curr_cb->block_size;
    if (cache->head == cache->tail) { // empty cache
        cache->head = NULL;
        cache->tail = NULL;
    } else if (curr_cb->prev == NULL) { // removing head
        cache->head = curr_cb->next;
        curr_cb->next->prev = NULL;
    } else if (curr_cb->next == NULL) { // removing tail
        cache->tail = curr_cb->prev;
        curr_cb->prev->next = NULL;
    } else { // a block in the middle
        curr_cb->prev->next = curr_cb->next;
        curr_cb->next->prev = curr_cb->prev;
    }
    if (curr_cb->key) {
        Free(curr_cb->key);
    }
    if (curr_cb->value) {
        Free(curr_cb->value);
    }
    Free(curr_cb);
}

/**
 * @brief Cleans up resources used by the web server cache after shutdown.
 * @param[in] cache pointer to the cache.
 *
 */
void free_cache(cache_t *cache) {
    // // remove cache blocks from the tail
    // while (cache->head->next != cache->tail) {
    //     evict_one_cb(cache, cache->tail->prev);
    // }
    // Free(cache->head);
    // Free(cache->tail);
    // Free(cache);
    cache_block_t *prev, *curr;
    curr = cache->head;
    while (curr) {
        prev = curr;
        curr = curr->next;
        evict_one_cb(cache, prev);
        // cache->cache_size -= prev->block_size;
        // if (prev->key) {
        //     Free(prev->key);
        // }
        // if (prev->value) {
        //     Free(prev->value);
        // }
        // Free(prev);
    }
    // cache->head = NULL;
    // cache->tail = NULL;
    Free(cache);
}

/**
 * @brief Insert a new block into cache with LRU eviction policy when not enough
 * space.
 * @param[in] cache pointer to the cache.
 * @param[in] key string stored as key for the block
 * @param[in] value string stored as value for the block
 * @param[in] buff_size size of the block value
 *
 * LRU policy is enforced by removing the tail block from the cache, since newly
 * added blocks and most recently referenced blocks are moved to the front of
 * the cache.
 */
void insert_cache(cache_t *cache, char *key, char *value, size_t buff_size) {
    pthread_mutex_lock(&mutex);
    // evict from tail when full until with enough space
    cache_block_t *cb_to_remove;
    while (buff_size > MAX_CACHE_SIZE - cache->cache_size) {
        cb_to_remove = cache->tail;
        evict_one_cb(cache, cb_to_remove);
    }

    // create a cache block to be added
    cache_block_t *cb_to_add = (cache_block_t *)Malloc(sizeof(cache_block_t));
    char *cb_key = NULL;
    char *cb_value = NULL;
    cb_key = (char *)Malloc(strlen(key) + 1);
    memcpy(cb_key, key, strlen(key) + 1);
    cb_value = (char *)Malloc(buff_size);
    memcpy(cb_value, value, buff_size);
    cb_to_add->key = cb_key;
    cb_to_add->value = cb_value;
    cb_to_add->block_size = buff_size;
    cb_to_add->next = NULL;
    cb_to_add->prev = NULL;

    /* add the new block as the head of cache */
    if (cache->cache_size == 0) { // when cache is still empty
        // cb_to_add->next = cb_to_add;
        // cb_to_add->prev = cb_to_add;
        cache->head = cb_to_add;
        cache->tail = cb_to_add;
        // cache->head->next = cb_to_add;
        // cache->head->prev = cb_to_add;
        // cache->tail->next = cb_to_add;
        // cache->tail->prev = cb_to_add;
    } else {
        // cache->head->prev = cb_to_add;
        // cache->tail->next = cb_to_add;
        // cb_to_add->next = cache->head;
        // cb_to_add->prev = cache->tail;
        // cache->head = cb_to_add;

        cb_to_add->next = cache->head;
        cache->head->prev = cb_to_add;
        cache->head = cb_to_add;
    }
    cache->cache_size += cb_to_add->block_size;
    // cache->num_block++;
    pthread_mutex_unlock(&mutex);
}

/**
 * @brief Private helper function to move the current block to the front.
 * @param[in] cache pointer to the cache.
 * @param[in] curr_cb cache block to be moved
 *
 */
static void move_to_front(cache_t *cache, cache_block_t *curr_cb) {
    if (curr_cb->prev != NULL) { // curr_cb is not head itself
        curr_cb->prev->next = curr_cb->next;
        curr_cb->next->prev = curr_cb->prev;
        curr_cb->next = cache->head;
        curr_cb->prev = NULL;
        cache->head = curr_cb;
    }
}

/**
 * @brief Iterate through cache to retrieve cached data if found in cache
 * @param[in] cache pointer to the cache.
 * @param[in] search_key string value of key to search
 * @param[in] value string pointer to store cached data
 *
 * To maintain LRU policy, retrieved cache block will be moved to the start of
 * the linked list, so that LRU blocks will be pushed to the end of the list.
 */
int retrieve_cache(cache_t *cache, char *search_key, char *value) {
    pthread_mutex_lock(&mutex);
    cache_block_t *curr_cb = cache->head;
    while (curr_cb) {
        if (!strcmp(curr_cb->key, search_key)) {
            memcpy(value, curr_cb->value, curr_cb->block_size);
            move_to_front(cache, curr_cb);
            pthread_mutex_unlock(&mutex);
            return 0;
        }
    }

    pthread_mutex_unlock(&mutex);
    return -1; // not found
}
