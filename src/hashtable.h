#ifndef HASHTABLE_H
#define HASHTABLE_H

#include <stddef.h>

#define INITIAL_BUCKETS_SIZE 16

typedef struct hashtable_pair
{
    void                  *key;
    size_t                 key_size;
    void                  *value;
    size_t                 value_size;
    struct hashtable_pair *next;
} hashtable_pair;

typedef struct hashtable
{
    hashtable_pair **buckets;
    size_t           buckets_size;
    size_t           pair_number;
    size_t         (*hash_function)(void *key);
    size_t         (*compare_key_function)(const void *key1, const void *key2);
} hashtable;

/**
 * @brief creates a new empty hashtable specifying hash and key comparison functions.
 * Allocated memory from the hashtable must be freed with `hashtable_destroy`.
 *
 * @param hash_function pointer to the hash function
 * @param compare_key_function pointer to the key comparison function
 * @return hashtable* pointer to the newly created hashtable
 */
hashtable *hashtable_create(size_t (*hash_function)(void *key),
                            size_t (*compare_key_function)(const void *key1, const void *key2));

/**
 * @brief inserts a new pair into the hashtable.
 * If the key already exists, its value is updated.
 *
 * @param ht pointer to the hashtable you want to insert the pair into
 * @param key pointer to the first byte of the key
 * @param key_size size of the key in bytes
 * @param value pointer to the first byte of the value
 * @param value_size size of the value in bytes
 * @return true if the pair was successfully inserted or updated
 * @return false if an error occurred (e.g., memory allocation failure)
 */
bool hashtable_put(hashtable *ht,
                   const void *key, const size_t key_size,
                   const void *value, const size_t value_size);

/**
 * @brief retrieves the value associated with the given key.
 * The returned pointer must be freed by the caller.
 *
 * @param ht pointer to the hashtable you want to retrieve the value from
 * @param key pointer to the first byte of the key
 * @return void* pointer to the value associated with the key, or NULL if the key does not exist
 */
void *hashtable_get(const hashtable *ht, const void *key);


/**
 * @brief removes the pair associated with the given key from the hashtable.
 *
 * @param ht pointer to the hashtable you want to remove the pair from
 * @param key pointer to the first byte of the key
 * @return true if the pair was successfully removed
 * @return false if the key does not exist in the hashtable
 */
bool hashtable_remove(hashtable *ht, const void *key);


/**
 * @brief returns an array of pointers to the keys stored in the hashtable.
 * The returned array must be freed by the caller.
 *
 * @param ht pointer to the hashtable you want to get the keys from
 * @return void** array of pointers to the keys stored in the hashtable
 */
void **hashtable_keyset(const hashtable *ht);

/**
 * @brief returns the number of pairs in the hashtable.
 *
 * @param ht pointer to the hashtable you want to get the size of
 * @param size pointer to a variable where the size will be stored
 * @return true if the size was successfully retrieved
 * @return false if an error occurred (e.g., invalid hashtable pointer)
 */
bool hashtable_size(const hashtable *ht, size_t *size);

/**
 * @brief destroys the hashtable and frees all allocated memory.
 *
 * @param ht pointer to the hashtable to destroy
 */
void hashtable_destroy(hashtable *ht);

/* ========== DEFAULT HASH FUNCTIONS ========== */
size_t hash_string(void *key);
size_t hash_int(void *key);
size_t hash_double(void *key);
/* ============================================ */

/* ========== DEFAULT KEY COMPARISON FUNCTIONS ========== */
size_t compare_string(const void *key1, const void *key2);
size_t compare_int(const void *key1, const void *key2);
size_t compare_double(const void *key1, const void *key2);
/* ====================================================== */

#endif // HASHTABLE_H
