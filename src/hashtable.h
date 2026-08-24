/*
 * hashtable - public interface.
 *
 * Naming: functions are hashtable_<verb>, the main type is hashtable_t, and
 * macros are COLLECTION_<MODULE>_<NAME>.
 *
 * Errors: return int status codes from collection_error.h, COLLECTION_OK
 * on success and a negative code on failure. Results go in out
 * parameters. Define an E_<MODULE>_<NAME> code counting down from
 * COLLECTION_EMODULE_BASE only when no shared code fits.
 *
 * Ownership: the caller owns the handle. Provide hashtable_init(hashtable_t *)
 * and hashtable_destroy(hashtable_t *) rather than allocating and returning
 * one.
 */

#ifndef COLLECTION_HASHTABLE_H
#define COLLECTION_HASHTABLE_H
#include "collection_api.h"
#include "collection_error.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief number of buckets a freshly initialized table starts with
 *
 * The table grows and shrinks on its own as pairs come and go, but never
 * shrinks below this many buckets, so a table that is emptied and refilled
 * does not pay for reallocating the bucket array from nothing every time.
 */
#define COLLECTION_HASHTABLE_INITIAL_BUCKETS 16

/* defined in hashtable.c, so the pair layout stays out of the ABI */
struct hashtable_pair;

/**
 * @brief hash of a key, deciding which bucket a pair lands in
 *
 * Handed to hashtable_init() and called with the bytes of a key and the size
 * those bytes were stored with. Return any size_t: the table reduces it to a
 * bucket index itself.
 *
 * The only hard requirement is consistency with the comparison the table was
 * initialized with. Two keys the comparison calls equal must hash the same,
 * otherwise a pair can be stored in one bucket and looked for in another and
 * the table will report it missing. The reverse is free: two keys may hash the
 * same without being equal, which is an ordinary collision and is handled.
 *
 * @note the key belongs to the table: a hash must not modify it, and must not
 * touch the table itself
 * @note pass NULL to hashtable_init() to get hashtable_hash_bytes()
 */
typedef size_t (*hashtable_hash_t)(const void *key, size_t key_size);

/**
 * @brief comparison deciding whether two keys are the same key
 *
 * Handed to hashtable_init() and called with the bytes of the key being looked
 * up and the bytes of a key already in the bucket, each with the size it was
 * stored with. Return 0 when the two are the same key and any non-zero value
 * when they are not.
 *
 * A hash table only ever asks whether two keys are equal, so, unlike the
 * comparison list_sort() takes, the sign of a non-zero result means nothing
 * and an ordering is not required.
 *
 * @note the keys belong to the table: a comparison must not modify them, and
 * must not touch the table itself
 * @note pass NULL to hashtable_init() to get hashtable_cmp_bytes()
 */
typedef int (*hashtable_cmp_t)(const void *key1, size_t key1_size,
                               const void *key2, size_t key2_size);

/**
 * @brief a table of byte-copied key/value pairs, looked up by key
 *
 * The caller owns the storage, so a hashtable_t can live on the stack, in
 * static storage or inside another struct. Initialize it with
 * hashtable_init() before any other call and release its pairs with
 * hashtable_destroy().
 *
 * A pair is entered with hashtable_put() and found again with
 * hashtable_get(), which copies the value out. Both the key and the value are
 * copied into the table, so neither has to outlive the call that stored it.
 * A key is present at most once: putting a key that is already there replaces
 * its value.
 *
 * Pairs are held in buckets chosen by the hash function, with collisions
 * chained, and the bucket array is resized as the table fills and empties, so
 * lookups stay flat as the table grows. The order pairs come back in from
 * hashtable_get_pair() follows that internal layout and is not the order they
 * were put in.
 *
 * @note the fields are internal. Read the number of pairs with
 * hashtable_get_size() instead of touching them directly
 */
typedef struct hashtable
{
    struct hashtable_pair **buckets;
    size_t                  buckets_size;
    size_t                  size;
    hashtable_hash_t        hash;
    hashtable_cmp_t         cmp;
    /* where hashtable_get_pair() resumes: the bucket to look at next, and the
       pair inside it, NULL once that bucket is exhausted */
    size_t                  cursor_bucket;
    struct hashtable_pair  *cursor_pair;
} hashtable_t;

/**
 * @brief initializes an empty hashtable with the given hash and comparison
 *
 * @param ht pointer to the hashtable to initialize, allocated by the caller
 * @param hash hash of a key, or NULL for hashtable_hash_bytes()
 * @param cmp comparison deciding whether two keys are the same key, or NULL
 * for hashtable_cmp_bytes()
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht is NULL,
 * COLLECTION_ENOMEM if the bucket array could not be allocated
 * @note hash and cmp have to agree: keys cmp calls equal must hash the same.
 * Passing NULL for both is the safe way to get a pair that does
 * @note unless ht itself was NULL, a failed call leaves ht a valid empty
 * table, so it can be reused or passed to hashtable_destroy() without leaking
 */
COLLECTION_API int hashtable_init(hashtable_t *ht, hashtable_hash_t hash,
                                  hashtable_cmp_t cmp);

/**
 * @brief frees every pair of the hashtable, leaving it empty
 *
 * @param ht pointer to the hashtable to empty
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht is NULL
 * @note the hashtable_t belongs to the caller and is not freed. Afterwards ht
 * is a valid empty table, holding the hash and the comparison it was
 * initialized with, and can be reused without calling hashtable_init() again
 * @note the iteration is rewound, so the next hashtable_get_pair() starts
 * over
 */
COLLECTION_API int hashtable_destroy(hashtable_t *ht);

/**
 * @brief stores a copy of a key and its value, replacing any value the key
 * already had
 *
 * @param ht pointer to the hashtable to store the pair in
 * @param key pointer to the data identifying the pair
 * @param key_size size of that data
 * @param value pointer to the data to store under the key
 * @param value_size size of that data
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht, key or value
 * is NULL, COLLECTION_EINVAL if key_size or value_size is 0,
 * COLLECTION_ENOMEM if the pair could not be allocated
 * @note replacing the value of a key that is already there does not change
 * the stored key or the size of the table, and the new value may be a
 * different size than the one it replaces
 * @note a failed put changes nothing: the key keeps the value it had
 * @note a successful put rewinds the iteration, because growing the table
 * moves pairs between buckets. See hashtable_get_pair()
 * @note ht, key and value must not overlap
 */
COLLECTION_API int hashtable_put(hashtable_t *restrict ht,
                                 const void *restrict key, size_t key_size,
                                 const void *restrict value,
                                 size_t value_size);

/**
 * @brief copies out the value stored under a key
 *
 * @param ht pointer to the hashtable to read from
 * @param key pointer to the data identifying the pair
 * @param key_size size of that data
 * @param value pointer to the storage the value is copied into
 * @param value_size size of that storage. It must equal the size the value
 * was stored with, so a value is never truncated and is never read beyond
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht, key or value
 * is NULL, COLLECTION_EINVAL if key_size or value_size is 0, or if value_size
 * does not match the size of the stored value, COLLECTION_ENOTFOUND if the
 * key is not in the table
 * @note a caller that does not already know the layout can ask
 * hashtable_get_value_size() how much storage to provide
 * @note ht, key and value must not overlap
 */
COLLECTION_API int hashtable_get(const hashtable_t *restrict ht,
                                 const void *restrict key, size_t key_size,
                                 void *restrict value, size_t value_size);

/**
 * @brief reads the size of the value stored under a key
 *
 * @param ht pointer to the hashtable to read from
 * @param key pointer to the data identifying the pair
 * @param key_size size of that data
 * @param value_size out parameter set to the size the value was stored with,
 * untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht, key or
 * value_size is NULL, COLLECTION_EINVAL if key_size is 0,
 * COLLECTION_ENOTFOUND if the key is not in the table
 * @note a table can hold values of different sizes. This is how a caller that
 * does not already know the layout finds out how much storage hashtable_get()
 * expects
 * @note ht, key and value_size must not overlap
 */
COLLECTION_API int hashtable_get_value_size(const hashtable_t *restrict ht,
                                            const void *restrict key,
                                            size_t key_size,
                                            size_t *restrict value_size);

/**
 * @brief tells whether a key is in the hashtable
 *
 * @param ht pointer to the hashtable to search
 * @param key pointer to the data identifying the pair
 * @param key_size size of that data
 * @param contains out parameter set to true when the key is in the table and
 * to false otherwise, untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht, key or
 * contains is NULL, COLLECTION_EINVAL if key_size is 0
 * @note a missing key is reported through contains, not as
 * COLLECTION_ENOTFOUND: asking whether a key is there succeeds either way
 * @note ht, key and contains must not overlap
 */
COLLECTION_API int hashtable_contains(const hashtable_t *restrict ht,
                                      const void *restrict key,
                                      size_t key_size, bool *restrict contains);

/**
 * @brief removes the pair stored under a key
 *
 * @param ht pointer to the hashtable to remove the pair from
 * @param key pointer to the data identifying the pair
 * @param key_size size of that data
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht or key is
 * NULL, COLLECTION_EINVAL if key_size is 0, COLLECTION_ENOTFOUND if the key
 * is not in the table
 * @note both the stored key and the stored value are freed
 * @note a successful remove rewinds the iteration, because shrinking the
 * table moves pairs between buckets. See hashtable_get_pair()
 * @note ht and key must not overlap
 */
COLLECTION_API int hashtable_remove(hashtable_t *restrict ht,
                                    const void *restrict key, size_t key_size);

/**
 * @brief copies out the next pair of the hashtable, advancing to the one
 * after it
 *
 * Every table carries one cursor over its pairs. The first call after the
 * table is initialized, rewound or changed hands back one pair and steps past
 * it, the next call hands back the pair after that, and so on until every
 * pair has been handed out once and the call reports COLLECTION_ENOTFOUND.
 *
 * @param ht pointer to the hashtable to take the pair from
 * @param key pointer to the storage the key is copied into
 * @param key_size size of that storage. It must equal the size the key was
 * stored with
 * @param value pointer to the storage the value is copied into
 * @param value_size size of that storage. It must equal the size the value
 * was stored with
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht, key or value
 * is NULL, COLLECTION_EINVAL if key_size or value_size is 0 or does not match
 * the size the next pair was stored with, COLLECTION_ENOTFOUND once the
 * iteration has handed out every pair
 * @note the cursor only moves when the call succeeds: a size mismatch leaves
 * the same pair waiting, so the caller can ask
 * hashtable_peek_pair_size() for the right sizes and try again
 * @note the pair is copied out, not removed. Iterating does not empty the
 * table
 * @note hashtable_put(), hashtable_remove() and hashtable_destroy() rewind
 * the cursor when they succeed, so a loop that changes the table while
 * walking it starts the walk over rather than skipping or repeating pairs at
 * random. Collect what you need first, then change the table
 * @note the order is the internal bucket order, not the order pairs were put
 * in, and it changes when the table is resized
 * @note ht, key and value must not overlap
 */
COLLECTION_API int hashtable_get_pair(hashtable_t *restrict ht,
                                      void *restrict key, size_t key_size,
                                      void *restrict value, size_t value_size);

/**
 * @brief reads the sizes of the pair hashtable_get_pair() would hand back
 * next, without advancing
 *
 * @param ht pointer to the hashtable to read from
 * @param key_size out parameter set to the size the next key was stored with,
 * untouched on failure
 * @param value_size out parameter set to the size the next value was stored
 * with, untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht, key_size or
 * value_size is NULL, COLLECTION_ENOTFOUND once the iteration has handed out
 * every pair
 * @note a table can hold keys and values of different sizes. This is how a
 * caller that does not already know the layout finds out how much storage
 * hashtable_get_pair() expects
 * @note ht, key_size and value_size must not overlap
 */
COLLECTION_API int hashtable_peek_pair_size(const hashtable_t *restrict ht,
                                            size_t *restrict key_size,
                                            size_t *restrict value_size);

/**
 * @brief rewinds the iteration, so the next hashtable_get_pair() starts over
 * from the first pair
 *
 * @param ht pointer to the hashtable to rewind
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht is NULL
 * @note the pairs are left alone: this moves the cursor and nothing else
 * @note rewinding a table that was never walked, or one that is already at
 * the start, is fine and does nothing
 */
COLLECTION_API int hashtable_rewind(hashtable_t *ht);

/**
 * @brief reads the number of pairs in the hashtable
 *
 * @param ht pointer to the hashtable you want to get the size of
 * @param size out parameter set to the number of pairs, untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht or size is
 * NULL
 * @note this is the number of pairs, not the number of buckets, which is
 * internal
 * @note ht and size must not overlap
 */
COLLECTION_API int hashtable_get_size(const hashtable_t *restrict ht,
                                      size_t *restrict size);

/**
 * @brief tells whether the hashtable holds no pairs
 *
 * @param ht pointer to the hashtable to inspect
 * @param empty out parameter set to true when the table holds no pairs and to
 * false otherwise, untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if ht or empty is
 * NULL
 * @note ht and empty must not overlap
 */
COLLECTION_API int hashtable_is_empty(const hashtable_t *restrict ht,
                                      bool *restrict empty);

/* ========== DEFAULT HASH FUNCTIONS ========== */

/**
 * @brief hashes a key by its raw bytes, the default hash
 *
 * This is what hashtable_init() uses when it is handed no hash, and it is the
 * one to reach for unless the keys need something else: it makes no
 * assumption about what the bytes mean, so it serves integers, floating point
 * numbers, fixed size buffers and plain structs alike.
 *
 * The bytes are folded with FNV-1a and the result is run through a bit mixer,
 * which is what keeps keys that differ in a single bit or only in their low
 * bits, such as consecutive integers or pointers, from piling into the same
 * few buckets.
 *
 * @param key pointer to the bytes to hash
 * @param key_size number of bytes to hash
 * @return size_t the hash, 0 when key is NULL or key_size is 0
 * @note two keys of different sizes hash differently even when the shorter
 * one is a prefix of the longer, which is what hashtable_cmp_bytes() expects
 * @note a struct with padding hashes by its padding too, so two structs with
 * equal members can hash differently. Hash the members, or clear the struct
 * with memset() before filling it in
 */
COLLECTION_API size_t hashtable_hash_bytes(const void *key, size_t key_size);

/**
 * @brief hashes a key as a string, stopping at the first NUL byte
 *
 * Same folding and mixing as hashtable_hash_bytes(), over the characters of
 * the string rather than over key_size raw bytes: at most key_size bytes are
 * read, and fewer when a NUL comes first. So "abc" stored with its terminator
 * and "abc" stored without it hash the same, which is what lets a caller
 * store keys with strlen() + 1 and look them up with strlen(), or the other
 * way round.
 *
 * @param key pointer to the string to hash
 * @param key_size number of bytes of key that may be read
 * @return size_t the hash, 0 when key is NULL or key_size is 0
 * @note pair it with hashtable_cmp_string(), which draws the same line at the
 * first NUL. Pairing it with hashtable_cmp_bytes() instead would leave "abc"
 * with and without its terminator hashing alike but comparing unequal, which
 * the table handles as an ordinary collision but which stores the same string
 * twice
 */
COLLECTION_API size_t hashtable_hash_string(const void *key, size_t key_size);

/* ============================================ */

/* ========== DEFAULT KEY COMPARISON FUNCTIONS ========== */

/**
 * @brief compares two keys by their raw bytes, the default comparison
 *
 * This is what hashtable_init() uses when it is handed no comparison. Keys of
 * different sizes are never the same key; keys of the same size are the same
 * key when every byte matches.
 *
 * @param key1 pointer to the bytes of the first key
 * @param key1_size size of the first key
 * @param key2 pointer to the bytes of the second key
 * @param key2_size size of the second key
 * @return int 0 when the two are the same key, non-zero when they are not
 * @note comparing floating point keys by their bytes is not comparing them as
 * numbers: 0.0 and -0.0 are equal as numbers but differ in their bytes, and a
 * NaN key matches the NaN with the same bytes even though NaN == NaN is false
 */
COLLECTION_API int hashtable_cmp_bytes(const void *key1, size_t key1_size,
                                       const void *key2, size_t key2_size);

/**
 * @brief compares two keys as strings, stopping at the first NUL byte
 *
 * The counterpart of hashtable_hash_string(): at most key1_size and key2_size
 * bytes are read, and the comparison ends at the first NUL, so the same
 * string is the same key whether or not the size it was stored with counted
 * the terminator.
 *
 * @param key1 pointer to the first string
 * @param key1_size number of bytes of key1 that may be read
 * @param key2 pointer to the second string
 * @param key2_size number of bytes of key2 that may be read
 * @return int 0 when the two are the same key, non-zero when they are not
 * @note a key whose bytes run out before a NUL is compared as the string it
 * spells up to that point, so no read ever goes past the size the key was
 * stored with
 */
COLLECTION_API int hashtable_cmp_string(const void *key1, size_t key1_size,
                                        const void *key2, size_t key2_size);

/* ====================================================== */

#endif // COLLECTION_HASHTABLE_H
