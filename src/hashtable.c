/*
 * hashtable - implementation.
 *
 * Everything that is not declared in hashtable.h must be `static`: the library
 * is built with hidden visibility and only COLLECTION_API declarations are
 * exported.
 */

#include "hashtable.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct hashtable_pair
{
    void                  *key;
    size_t                 key_size;
    void                  *value;
    size_t                 value_size;
    struct hashtable_pair *next;
};

/* -- PAIRS -------------------------------------------------------------- */

static struct hashtable_pair *pair_create(const void *key, size_t key_size,
                                          const void *value, size_t value_size)
{
    struct hashtable_pair *p = malloc(sizeof(struct hashtable_pair));
    if (p == NULL) return NULL;

    p->key = malloc(key_size);
    if (p->key == NULL)
    {
        free(p);
        return NULL;
    }

    p->value = malloc(value_size);
    if (p->value == NULL)
    {
        free(p->key);
        free(p);
        return NULL;
    }

    memcpy(p->key, key, key_size);
    p->key_size = key_size;

    memcpy(p->value, value, value_size);
    p->value_size = value_size;

    p->next = NULL;

    return p;
}

static void pair_destroy(struct hashtable_pair *p)
{
    if (p == NULL) return;
    free(p->key);
    free(p->value);
    free(p);
}

/* -- BUCKETS ------------------------------------------------------------ */

/* the bucket a key belongs in. The count is always a power of two, so the
   index is the low bits of the hash rather than a division */
static size_t bucket_of(const hashtable_t *ht, const void *key, size_t key_size)
{
    return ht->hash(key, key_size) & (ht->buckets_size - 1);
}

/*
 * the address of the pointer that links to the pair stored under key: the
 * bucket head when the pair is first in its chain, and the next field of the
 * pair before it otherwise. NULL when the key is not in the table.
 *
 * Handing back the link rather than the pair is what lets hashtable_remove()
 * unlink a pair without walking the chain a second time and without a special
 * case for the one at the front.
 */
static struct hashtable_pair **pair_link(const hashtable_t *ht, size_t index,
                                         const void *key, size_t key_size)
{
    struct hashtable_pair **link = &ht->buckets[index];

    while (*link != NULL)
    {
        if (ht->cmp(key, key_size, (*link)->key, (*link)->key_size) == 0)
            return link;

        link = &(*link)->next;
    }

    return NULL;
}

/* moves every pair into a bucket array of new_size, which must be a power of
   two. The pairs are relinked rather than reallocated, so a resize copies no
   keys and no values */
static int buckets_resize(hashtable_t *ht, size_t new_size)
{
    struct hashtable_pair **buckets = calloc(new_size, sizeof(*buckets));
    if (buckets == NULL) return COLLECTION_ENOMEM;

    for (size_t i = 0; i < ht->buckets_size; i++)
    {
        struct hashtable_pair *p = ht->buckets[i];

        while (p != NULL)
        {
            struct hashtable_pair *next  = p->next;
            size_t                 index = ht->hash(p->key, p->key_size) &
                                           (new_size - 1);

            p->next        = buckets[index];
            buckets[index] = p;
            p              = next;
        }
    }

    free(ht->buckets);
    ht->buckets      = buckets;
    ht->buckets_size = new_size;

    return COLLECTION_OK;
}

/* the bucket array a table needs before it can hold anything. hashtable_init()
   normally leaves one behind; this is for the table whose init, or whose
   destroy, could not allocate it, so a table that ran out of memory once is
   still usable rather than broken for good */
static int buckets_ensure(hashtable_t *ht)
{
    if (ht->buckets != NULL) return COLLECTION_OK;

    ht->buckets = calloc(COLLECTION_HASHTABLE_INITIAL_BUCKETS,
                         sizeof(*ht->buckets));
    if (ht->buckets == NULL) return COLLECTION_ENOMEM;

    ht->buckets_size = COLLECTION_HASHTABLE_INITIAL_BUCKETS;

    return COLLECTION_OK;
}

/* -- THE CURSOR --------------------------------------------------------- */

/*
 * the pair hashtable_get_pair() would hand back next, with the bucket it sits
 * in written to bucket. NULL once the walk is over.
 *
 * The cursor is a bucket and, when the walk is part way down a chain, the
 * pair inside it; a cursor with no pair means the next one is the head of the
 * first non-empty bucket from there on. Working it out on the way past like
 * this, rather than storing it, is what lets hashtable_peek_pair_size() read
 * the cursor without moving it.
 */
static struct hashtable_pair *cursor_next(const hashtable_t *ht,
                                          size_t *bucket)
{
    if (ht->cursor_pair != NULL)
    {
        *bucket = ht->cursor_bucket;
        return ht->cursor_pair;
    }

    for (size_t i = ht->cursor_bucket; i < ht->buckets_size; i++)
    {
        if (ht->buckets[i] == NULL) continue;

        *bucket = i;
        return ht->buckets[i];
    }

    return NULL;
}

/* leaves the cursor on whatever follows pair, which was in bucket: the rest
   of its chain first, then the buckets after it */
static void cursor_advance(hashtable_t *ht, size_t bucket,
                           const struct hashtable_pair *pair)
{
    if (pair->next != NULL)
    {
        ht->cursor_bucket = bucket;
        ht->cursor_pair   = pair->next;
        return;
    }

    ht->cursor_bucket = bucket + 1;
    ht->cursor_pair   = NULL;
}

/* -- THE TABLE ---------------------------------------------------------- */

int hashtable_init(hashtable_t *ht, hashtable_hash_t hash, hashtable_cmp_t cmp)
{
    if (ht == NULL) return COLLECTION_ENULL;

    ht->buckets      = NULL;
    ht->buckets_size = 0;
    ht->size         = 0;
    ht->hash         = (hash != NULL) ? hash : hashtable_hash_bytes;
    ht->cmp          = (cmp != NULL) ? cmp : hashtable_cmp_bytes;

    hashtable_rewind(ht);

    /* on failure the table is left empty and without an array, which is a
       state every other call handles: the lookups find nothing in it and a
       put allocates it before storing anything */
    return buckets_ensure(ht);
}

int hashtable_destroy(hashtable_t *ht)
{
    if (ht == NULL) return COLLECTION_ENULL;

    for (size_t i = 0; i < ht->buckets_size; i++)
    {
        struct hashtable_pair *p = ht->buckets[i];

        while (p != NULL)
        {
            struct hashtable_pair *next = p->next;

            pair_destroy(p);
            p = next;
        }
    }

    /* the bucket array goes too, so a destroyed table owns nothing at all and
       the caller can drop the handle. buckets_ensure() makes a new one the
       next time something is put in, which is what keeps the table reusable
       without holding on to memory in the meantime */
    free(ht->buckets);
    ht->buckets      = NULL;
    ht->buckets_size = 0;
    ht->size         = 0;

    hashtable_rewind(ht);

    return COLLECTION_OK;
}

int hashtable_put(hashtable_t *restrict ht, const void *restrict key,
                  size_t key_size, const void *restrict value,
                  size_t value_size)
{
    if (ht == NULL || key == NULL || value == NULL) return COLLECTION_ENULL;
    if (key_size == 0 || value_size == 0) return COLLECTION_EINVAL;

    int rc = buckets_ensure(ht);
    if (rc != COLLECTION_OK) return rc;

    /* the copies are made before the table is touched at all, so a put that
       cannot allocate has changed nothing. It also means a size no allocator
       can serve is turned down before anything tries to read that many bytes
       of the caller's key */
    struct hashtable_pair *new = pair_create(key, key_size, value, value_size);
    if (new == NULL) return COLLECTION_ENOMEM;

    size_t                  index = bucket_of(ht, key, key_size);
    struct hashtable_pair **link  = pair_link(ht, index, key, key_size);

    if (link != NULL)
    {
        /* the key is already here, so it keeps the key it was stored with and
           takes over the value just allocated. The old value is released only
           now, with the new one already in hand: nothing past this point can
           fail and leave the pair without a value */
        struct hashtable_pair *stored = *link;

        free(stored->value);
        stored->value      = new->value;
        stored->value_size = new->value_size;

        new->value = NULL;
        pair_destroy(new);

        hashtable_rewind(ht);
        return COLLECTION_OK;
    }

    /* a new key goes at the front of its chain, which costs nothing and is
       where a key that was just stored is most likely to be looked for */
    new->next          = ht->buckets[index];
    ht->buckets[index] = new;
    ht->size++;

    /* past three quarters full the chains start to grow, so the array
       doubles. A resize that cannot allocate is not a failed put: the pair is
       in, and the table simply stays as loaded as it is */
    if (ht->size > ht->buckets_size / 4 * 3)
        (void)buckets_resize(ht, ht->buckets_size * 2);

    /* a resize moves pairs between buckets, so a walk in progress can no
       longer be trusted to visit each of them once */
    hashtable_rewind(ht);

    return COLLECTION_OK;
}

int hashtable_get(const hashtable_t *restrict ht, const void *restrict key,
                  size_t key_size, void *restrict value, size_t value_size)
{
    if (ht == NULL || key == NULL || value == NULL) return COLLECTION_ENULL;
    if (key_size == 0 || value_size == 0) return COLLECTION_EINVAL;
    if (ht->size == 0) return COLLECTION_ENOTFOUND;

    struct hashtable_pair **link =
        pair_link(ht, bucket_of(ht, key, key_size), key, key_size);
    if (link == NULL) return COLLECTION_ENOTFOUND;

    /* the destination has to be exactly the value, so nothing is truncated
       and memcpy never reads past either object */
    if ((*link)->value_size != value_size) return COLLECTION_EINVAL;

    memcpy(value, (*link)->value, value_size);

    return COLLECTION_OK;
}

int hashtable_get_value_size(const hashtable_t *restrict ht,
                             const void *restrict key, size_t key_size,
                             size_t *restrict value_size)
{
    if (ht == NULL || key == NULL || value_size == NULL)
        return COLLECTION_ENULL;
    if (key_size == 0) return COLLECTION_EINVAL;
    if (ht->size == 0) return COLLECTION_ENOTFOUND;

    struct hashtable_pair **link =
        pair_link(ht, bucket_of(ht, key, key_size), key, key_size);
    if (link == NULL) return COLLECTION_ENOTFOUND;

    *value_size = (*link)->value_size;

    return COLLECTION_OK;
}

int hashtable_contains(const hashtable_t *restrict ht,
                       const void *restrict key, size_t key_size,
                       bool *restrict contains)
{
    if (ht == NULL || key == NULL || contains == NULL) return COLLECTION_ENULL;
    if (key_size == 0) return COLLECTION_EINVAL;

    if (ht->size == 0)
    {
        *contains = false;
        return COLLECTION_OK;
    }

    *contains =
        pair_link(ht, bucket_of(ht, key, key_size), key, key_size) != NULL;

    return COLLECTION_OK;
}

int hashtable_remove(hashtable_t *restrict ht, const void *restrict key,
                     size_t key_size)
{
    if (ht == NULL || key == NULL) return COLLECTION_ENULL;
    if (key_size == 0) return COLLECTION_EINVAL;
    if (ht->size == 0) return COLLECTION_ENOTFOUND;

    struct hashtable_pair **link =
        pair_link(ht, bucket_of(ht, key, key_size), key, key_size);
    if (link == NULL) return COLLECTION_ENOTFOUND;

    struct hashtable_pair *stored = *link;

    /* whatever pointed at the pair now points past it, whether that was the
       bucket head or the pair before it */
    *link = stored->next;

    pair_destroy(stored);
    ht->size--;

    /* below a quarter full the array is mostly empty, so it halves. The gap
       between that and the three quarters it takes to grow again is what
       keeps a table sitting near a threshold from resizing on every call. As
       with a grow, a resize that cannot allocate is not a failed remove */
    if (ht->size < ht->buckets_size / 4 &&
        ht->buckets_size > COLLECTION_HASHTABLE_INITIAL_BUCKETS)
        (void)buckets_resize(ht, ht->buckets_size / 2);

    /* the cursor may have been sitting on the pair that was just freed, and
       a resize moves the rest of them around */
    hashtable_rewind(ht);

    return COLLECTION_OK;
}

int hashtable_get_pair(hashtable_t *restrict ht, void *restrict key,
                       size_t key_size, void *restrict value,
                       size_t value_size)
{
    if (ht == NULL || key == NULL || value == NULL) return COLLECTION_ENULL;
    if (key_size == 0 || value_size == 0) return COLLECTION_EINVAL;

    size_t                 bucket = 0;
    struct hashtable_pair *pair   = cursor_next(ht, &bucket);

    if (pair == NULL) return COLLECTION_ENOTFOUND;

    if (pair->key_size != key_size || pair->value_size != value_size)
        return COLLECTION_EINVAL;

    memcpy(key, pair->key, key_size);
    memcpy(value, pair->value, value_size);

    /* the copies happen first, so a rejected call leaves the same pair
       waiting for a caller that asks with the right sizes */
    cursor_advance(ht, bucket, pair);

    return COLLECTION_OK;
}

int hashtable_peek_pair_size(const hashtable_t *restrict ht,
                             size_t *restrict key_size,
                             size_t *restrict value_size)
{
    if (ht == NULL || key_size == NULL || value_size == NULL)
        return COLLECTION_ENULL;

    size_t                 bucket = 0;
    struct hashtable_pair *pair   = cursor_next(ht, &bucket);

    if (pair == NULL) return COLLECTION_ENOTFOUND;

    *key_size   = pair->key_size;
    *value_size = pair->value_size;

    return COLLECTION_OK;
}

int hashtable_rewind(hashtable_t *ht)
{
    if (ht == NULL) return COLLECTION_ENULL;

    ht->cursor_bucket = 0;
    ht->cursor_pair   = NULL;

    return COLLECTION_OK;
}

int hashtable_get_size(const hashtable_t *restrict ht, size_t *restrict size)
{
    if (ht == NULL || size == NULL) return COLLECTION_ENULL;

    *size = ht->size;

    return COLLECTION_OK;
}

int hashtable_is_empty(const hashtable_t *restrict ht, bool *restrict empty)
{
    if (ht == NULL || empty == NULL) return COLLECTION_ENULL;

    *empty = (ht->size == 0);

    return COLLECTION_OK;
}

/* -- DEFAULT HASH FUNCTIONS --------------------------------------------- */

/*
 * FNV-1a, then a bit mixer.
 *
 * FNV-1a folds the bytes in a single pass with no table and no alignment
 * requirement, which is what makes it a reasonable default for keys of any
 * shape and any size. On its own it avalanches poorly at the bottom end:
 * keys that differ only in their last few bits, such as consecutive integers
 * or addresses, come out with hashes just as close together, and the bucket
 * index is taken from exactly those low bits.
 *
 * The mixer is the finalizer from MurmurHash3, two rounds of shift, xor and
 * multiply, which carries every input bit into every output bit. That is what
 * makes masking the result down to a bucket safe, and it is what the tests
 * counting occupied buckets are measuring.
 *
 * Both halves are public domain, and neither reads a key as anything wider
 * than a byte, so the same key hashes the same on every platform.
 */
static uint64_t fnv1a(const unsigned char *bytes, size_t count)
{
    /* the 64 bit FNV-1a offset basis */
    uint64_t hash = 0xcbf29ce484222325ull;

    for (size_t i = 0; i < count; i++)
    {
        hash ^= bytes[i];
        hash *= 0x00000100000001b3ull; /* the 64 bit FNV prime */
    }

    return hash;
}

static uint64_t mix(uint64_t hash)
{
    hash ^= hash >> 33;
    hash *= 0xff51afd7ed558ccdull;
    hash ^= hash >> 33;
    hash *= 0xc4ceb9fe1a85ec53ull;
    hash ^= hash >> 33;

    return hash;
}

size_t hashtable_hash_bytes(const void *key, size_t key_size)
{
    if (key == NULL || key_size == 0) return 0;

    /* on a 32 bit size_t this keeps the low half, which the mixer has already
       made depend on the whole key */
    return (size_t)mix(fnv1a(key, key_size));
}

size_t hashtable_hash_string(const void *key, size_t key_size)
{
    if (key == NULL || key_size == 0) return 0;

    const unsigned char *bytes  = key;
    size_t               length = 0;

    /* the key ends at the first NUL or at the size it was stored with,
       whichever comes first, so the terminator is never part of the hash and
       no read runs past what the caller owns */
    while (length < key_size && bytes[length] != '\0')
        length++;

    return (size_t)mix(fnv1a(bytes, length));
}

/* -- DEFAULT KEY COMPARISON FUNCTIONS ----------------------------------- */

int hashtable_cmp_bytes(const void *key1, size_t key1_size, const void *key2,
                        size_t key2_size)
{
    /* keys of different lengths are different keys, whatever their bytes say,
       which is what keeps a short key from matching the start of a long one */
    if (key1_size != key2_size) return 1;
    if (key1 == NULL || key2 == NULL) return key1 != key2;

    return memcmp(key1, key2, key1_size);
}

int hashtable_cmp_string(const void *key1, size_t key1_size, const void *key2,
                         size_t key2_size)
{
    if (key1 == NULL || key2 == NULL) return key1 != key2;

    const unsigned char *a = key1;
    const unsigned char *b = key2;

    /* walks both at once and stops at the first difference, at the NUL that
       ends either string, or at the size either key was stored with. Reading
       a key that has run out as a NUL is what makes "abc" stored with its
       terminator and "abc" stored without it the same key */
    for (size_t i = 0;; i++)
    {
        const unsigned char ca = (i < key1_size) ? a[i] : '\0';
        const unsigned char cb = (i < key2_size) ? b[i] : '\0';

        if (ca != cb) return (int)ca - (int)cb;
        if (ca == '\0') return 0;
    }
}
