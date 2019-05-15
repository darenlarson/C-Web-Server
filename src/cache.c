#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include "cache.h"

/**
 * Allocate a cache entry
 */
struct cache_entry *alloc_entry(char *path, char *content_type, void *content, int content_length) {
    struct cache_entry *new_entry = malloc(sizeof(struct cache_entry));

    new_entry->path = malloc(strlen(path) + 1);
    strcpy(new_entry->path, path);  // key to the cache

    new_entry->content_type = malloc(strlen(content_type) + 1);
    strcpy(new_entry->content_type, content_type);

    new_entry->content = malloc(content_length);
    memcpy(new_entry->content, content, content_length);

    new_entry->content_length = content_length;

    new_entry->prev = NULL;
    new_entry->next = NULL;

    return new_entry;
}

/** 
 * Deallocate a cache entry
 */
void free_entry(struct cache_entry *entry) {
    free(entry->path);
    free(entry->content_type);
    free(entry->content);
    free(entry);
}

/**
 * Insert a cache entry at the head of the linked list
 */
void dllist_insert_head(struct cache *cache, struct cache_entry *ce) {
    // Insert at the head of the list
    if (cache->head == NULL) {
        cache->head = cache->tail = ce;
        ce->prev = ce->next = NULL;
    } else {
        cache->head->prev = ce;
        ce->next = cache->head;
        ce->prev = NULL;
        cache->head = ce;
    }
}

/**
 * Move a cache entry to the head of the list
 */
void dllist_move_to_head(struct cache *cache, struct cache_entry *ce) {
    if (ce != cache->head) {
        if (ce == cache->tail) {
            // We're the tail
            cache->tail = ce->prev;
            cache->tail->next = NULL;

        } else {
            // We're neither the head nor the tail
            ce->prev->next = ce->next;
            ce->next->prev = ce->prev;
        }

        ce->next = cache->head;
        cache->head->prev = ce;
        ce->prev = NULL;
        cache->head = ce;
    }
}


/**
 * Removes the tail from the list and returns it
 * 
 * NOTE: does not deallocate the tail
 */
struct cache_entry *dllist_remove_tail(struct cache *cache) {
    struct cache_entry *oldtail = cache->tail;

    cache->tail = oldtail->prev;
    cache->tail->next = NULL;

    cache->cur_size--;

    return oldtail;
}

/**
 * Create a new cache
 * 
 * max_size: maximum number of entries in the cache
 * hashsize: hashtable size (0 for default)
 */
struct cache *cache_create(int max_size, int hashsize) {
    struct cache *new_cache = malloc(sizeof(struct cache));
    struct hashtable *hash_table = hashtable_create(hashsize, NULL);

    new_cache->head = new_cache->tail = NULL;
    new_cache->cur_size = 0;
    new_cache->max_size = max_size;
    new_cache->index = hash_table;

    return new_cache;
}

void cache_free(struct cache *cache) {
    struct cache_entry *cur_entry = cache->head;

    hashtable_destroy(cache->index);

    while (cur_entry != NULL) {
        struct cache_entry *next_entry = cur_entry->next;

        free_entry(cur_entry);

        cur_entry = next_entry;
    }

    free(cache);
}

// Store an entry in the cache
// This will also remove the least-recently-used items as necessary.
// NOTE: doesn't check for duplicate cache entries
void cache_put(struct cache *cache, char *path, char *content_type, void *content, int content_length) {
    struct cache_entry *new_entry = alloc_entry(path, content_type, content, content_length); // creates new cache entry struct

    dllist_insert_head(cache, new_entry); // adds the cache entry to the head (since that's where the most recently used entry goes)

    hashtable_put(cache->index, path, new_entry); // Put to hashtable with the path string as the key

    cache->cur_size += 1;

    if (cache->cur_size > cache->max_size) {
        struct cache_entry *LRU_entry = dllist_remove_tail(cache); // removes the tail entry in the linked list, since that's the oldest entry
        
        hashtable_delete(cache->index, LRU_entry->path); // deletes this entry from the hashtable
        
        free_entry(LRU_entry);
        
        // cache->cur_size -= 1; // decrement cur_size since we just removed 1 entry. It should now equal max_size
        if (cache->cur_size > cache->max_size) {
            cache->cur_size -= 1;
        }
    }


}

/**
 * Retrieve an entry from the cache
 */
struct cache_entry *cache_get(struct cache *cache, char *path) {
    if (hashtable_get(cache->index, path) == NULL) { // Attempt to find the cache entry pointer by path in the hash table
        return NULL;

    } else {
        dllist_move_to_head(cache, hashtable_get(cache->index, path)); // Move the cache entry to the head of the doubly-linked list

        return cache->head; // Return the cache entry pointer
    }
}
