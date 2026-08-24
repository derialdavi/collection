/*
 * Unit tests for hashtable.
 *
 * Name cases test_hashtable_should_<Behaviour>, one behaviour per case.
 */

#ifdef TEST

#include "unity.h"

#include "hashtable.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/*
 * hashtable.h publishes struct hashtable but keeps struct hashtable_pair
 * opaque, so there is no public way to walk a bucket. These tests mirror the
 * definition from hashtable.c in order to check the chains directly.
 *
 * KEEP IN SYNC WITH hashtable.c. If the layout there changes and this does
 * not, every check below reads the wrong bytes and can still pass.
 */
struct hashtable_pair
{
    void                  *key;
    size_t                 key_size;
    void                  *value;
    size_t                 value_size;
    struct hashtable_pair *next;
};

static hashtable_t ht;

/* how many times the counting hash and comparison below were called, so a
   test can prove the table used the callbacks it was handed */
static size_t hash_calls;
static size_t cmp_calls;

void setUp(void)
{
    /* poison the handle so a field hashtable_init forgets to write shows up
       as garbage rather than as a lucky zero */
    memset(&ht, 0xAA, sizeof(ht));

    hash_calls = 0;
    cmp_calls  = 0;
}

void tearDown(void)
{
    /* nothing: a test that allocates pairs destroys them itself, and the
       poisoned handle above must not be walked by a blind destroy */
}

/* -- CALLBACKS USED BY THE TESTS ---------------------------------------- */

/* the byte defaults, wrapped so a test can count the calls the table makes */
static size_t hash_counting(const void *key, size_t key_size)
{
    hash_calls++;
    return hashtable_hash_bytes(key, key_size);
}

static int cmp_counting(const void *key1, size_t key1_size, const void *key2,
                        size_t key2_size)
{
    cmp_calls++;
    return hashtable_cmp_bytes(key1, key1_size, key2, key2_size);
}

/* sends every key to the same bucket, so the table becomes one long chain.
   The way to exercise collision handling on purpose rather than by luck */
static size_t hash_always_zero(const void *key, size_t key_size)
{
    (void)key;
    (void)key_size;
    return 0;
}

/* -- SHARED FIXTURES AND ASSERTIONS ------------------------------------- */

/* a key/value pair of ints, which is what most of these tests store */
struct int_pair
{
    int key;
    int value;
};

/* asserts that the iteration is where hashtable_init(), hashtable_destroy()
   and hashtable_rewind() promise to leave it: about to hand back the first
   pair of the first non-empty bucket */
static void assert_at_start(const hashtable_t *h)
{
    TEST_ASSERT_EQUAL_size_t(0, h->cursor_bucket);
    TEST_ASSERT_NULL(h->cursor_pair);
}

/* asserts that the bucket array really holds h->size pairs, each of them a
   well formed one. Everything else is checked on top of this, so a table that
   loses or double counts a pair fails here first */
static void assert_bookkeeping(const hashtable_t *h)
{
    if (h->buckets == NULL)
    {
        /* a table that owns no bucket array can hold nothing in it */
        TEST_ASSERT_EQUAL_size_t(0, h->buckets_size);
        TEST_ASSERT_EQUAL_size_t(0, h->size);
        return;
    }

    TEST_ASSERT_TRUE(h->buckets_size >= COLLECTION_HASHTABLE_INITIAL_BUCKETS);
    /* the index is taken by masking, which only works on a power of two */
    TEST_ASSERT_EQUAL_size_t(0, h->buckets_size & (h->buckets_size - 1));

    size_t seen = 0;

    for (size_t i = 0; i < h->buckets_size; i++)
    {
        for (struct hashtable_pair *p = h->buckets[i]; p != NULL; p = p->next)
        {
            TEST_ASSERT_NOT_NULL(p->key);
            TEST_ASSERT_NOT_NULL(p->value);
            TEST_ASSERT_TRUE(p->key_size > 0);
            TEST_ASSERT_TRUE(p->value_size > 0);

            seen++;
            /* a cycle in a chain would loop here forever otherwise */
            TEST_ASSERT_TRUE(seen <= h->size);
        }
    }

    TEST_ASSERT_EQUAL_size_t(h->size, seen);
}

/* asserts that h is the empty table hashtable_init() and hashtable_destroy()
   promise */
static void assert_empty(const hashtable_t *h)
{
    TEST_ASSERT_EQUAL_size_t(0, h->size);
    TEST_ASSERT_EQUAL_size_t(COLLECTION_HASHTABLE_INITIAL_BUCKETS,
                             h->buckets_size);
    TEST_ASSERT_NOT_NULL(h->buckets);

    for (size_t i = 0; i < h->buckets_size; i++)
        TEST_ASSERT_NULL(h->buckets[i]);

    assert_at_start(h);
    assert_bookkeeping(h);
}

/* asserts that h is the released table hashtable_destroy() promises: empty,
   still set up, and owning nothing the caller would have to free */
static void assert_released(const hashtable_t *h)
{
    TEST_ASSERT_EQUAL_size_t(0, h->size);
    TEST_ASSERT_NULL(h->buckets);
    TEST_ASSERT_EQUAL_size_t(0, h->buckets_size);

    assert_at_start(h);
    assert_bookkeeping(h);
}

/* asserts that h holds exactly the count int pairs in expected, by asking for
   every one of them back. The order they sit in is the table's business, so
   this says nothing about it */
static void assert_contents(const hashtable_t *h, const struct int_pair *expected,
                            size_t count)
{
    TEST_ASSERT_EQUAL_size_t(count, h->size);
    assert_bookkeeping(h);

    for (size_t i = 0; i < count; i++)
    {
        int out = 0;

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_get(h, &expected[i].key,
                                            sizeof(expected[i].key), &out,
                                            sizeof(out)));
        TEST_ASSERT_EQUAL_INT(expected[i].value, out);
    }
}

/* rewinds h and walks it to the end, asserting that hashtable_get_pair()
   hands back exactly the count int pairs in expected, each of them once.
   Which order they come in is not part of the contract, so expected is
   treated as a set */
static void assert_walk_yields(hashtable_t *h, const struct int_pair *expected,
                               size_t count)
{
    /* at most as many flags as there are pairs, sized for the largest table
       these tests build */
    bool seen[64] = { false };

    TEST_ASSERT_TRUE(count <= 64);
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_rewind(h));

    for (size_t i = 0; i < count; i++)
    {
        int key   = 0;
        int value = 0;

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_get_pair(h, &key, sizeof(key), &value,
                                                 sizeof(value)));

        bool matched = false;

        for (size_t j = 0; j < count; j++)
        {
            if (seen[j] || expected[j].key != key) continue;

            TEST_ASSERT_EQUAL_INT(expected[j].value, value);
            seen[j] = true;
            matched = true;
            break;
        }

        /* a key that is not in expected, or one handed back twice */
        TEST_ASSERT_TRUE_MESSAGE(matched, "unexpected or repeated key");
    }

    /* the walk is over: nothing is left and nothing is written */
    int key   = 0x1234;
    int value = 0x5678;

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_get_pair(h, &key, sizeof(key), &value,
                                             sizeof(value)));
    TEST_ASSERT_EQUAL_INT(0x1234, key);
    TEST_ASSERT_EQUAL_INT(0x5678, value);
}

/* fixture: an initialized table holding count int pairs, built with the API
   and with the default hash and comparison */
static void fill(hashtable_t *h, const struct int_pair *pairs, size_t count)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(h, NULL, NULL));

    for (size_t i = 0; i < count; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(h, &pairs[i].key,
                                            sizeof(pairs[i].key),
                                            &pairs[i].value,
                                            sizeof(pairs[i].value)));

    assert_contents(h, pairs, count);
}

/* the number of pairs in the chain of bucket index */
static size_t chain_length(const hashtable_t *h, size_t index)
{
    size_t length = 0;

    for (struct hashtable_pair *p = h->buckets[index]; p != NULL; p = p->next)
        length++;

    return length;
}

/* -- INIT --------------------------------------------------------------- */

void test_hashtable_should_InitAnEmptyTableWithABucketArrayReady(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    assert_empty(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_DefaultToTheByteHashAndComparisonWhenGivenNeither(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    TEST_ASSERT_EQUAL_PTR(hashtable_hash_bytes, ht.hash);
    TEST_ASSERT_EQUAL_PTR(hashtable_cmp_bytes, ht.cmp);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_KeepTheHashAndComparisonItWasGiven(void)
{
    const int key   = 42;
    const int value = 7;
    int       out   = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_init(&ht, hash_counting, cmp_counting));

    TEST_ASSERT_EQUAL_PTR(hash_counting, ht.hash);
    TEST_ASSERT_EQUAL_PTR(cmp_counting, ht.cmp);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &value,
                                        sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, &key, sizeof(key), &out,
                                        sizeof(out)));

    /* they are not just stored, they are the ones actually used */
    TEST_ASSERT_TRUE(hash_calls > 0);
    TEST_ASSERT_TRUE(cmp_calls > 0);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_FillInOnlyTheMissingHalfOfThePair(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_init(&ht, hash_counting, NULL));
    TEST_ASSERT_EQUAL_PTR(hash_counting, ht.hash);
    TEST_ASSERT_EQUAL_PTR(hashtable_cmp_bytes, ht.cmp);
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));

    memset(&ht, 0xAA, sizeof(ht));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_init(&ht, NULL, cmp_counting));
    TEST_ASSERT_EQUAL_PTR(hashtable_hash_bytes, ht.hash);
    TEST_ASSERT_EQUAL_PTR(cmp_counting, ht.cmp);
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectANullTablePointerOnInit(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, hashtable_init(NULL, NULL, NULL));
}

/* -- DESTROY ------------------------------------------------------------ */

void test_hashtable_should_EmptyTheTableOnDestroyAndLeaveItReusable(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };

    fill(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
    assert_released(&ht);

    /* usable again without a second hashtable_init, which means putting a
       pair into a table that no longer owns a bucket array */
    const struct int_pair again = { 9, 90 };

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &again.key, sizeof(again.key),
                                        &again.value, sizeof(again.value)));
    assert_contents(&ht, &again, 1);

    /* and destroying an already destroyed table is harmless */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
    assert_released(&ht);
}

void test_hashtable_should_KeepTheHashAndComparisonAcrossDestroy(void)
{
    const struct int_pair pair = { 1, 10 };

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_init(&ht, hash_counting, cmp_counting));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &pair.key, sizeof(pair.key),
                                        &pair.value, sizeof(pair.value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));

    /* the callbacks belong to the table, not to the pairs it happened to
       hold, so a reused table still hashes the way it was set up to */
    TEST_ASSERT_EQUAL_PTR(hash_counting, ht.hash);
    TEST_ASSERT_EQUAL_PTR(cmp_counting, ht.cmp);

    hash_calls = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &pair.key, sizeof(pair.key),
                                        &pair.value, sizeof(pair.value)));
    TEST_ASSERT_TRUE(hash_calls > 0);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReleaseTheBucketArrayOnDestroy(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    for (int i = 0; i < 40; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, &i, sizeof(i), &i, sizeof(i)));

    TEST_ASSERT_TRUE(ht.buckets_size > COLLECTION_HASHTABLE_INITIAL_BUCKETS);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));

    /* a destroyed table owns nothing: the array it grew is gone too, so a
       caller that is done with it can drop the handle and leak nothing. The
       array the table started with is not kept either, since there would be
       no call left that could ever release it */
    assert_released(&ht);

    /* and it builds itself a new one when it is next used */
    const struct int_pair again = { 1, 10 };

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &again.key, sizeof(again.key),
                                        &again.value, sizeof(again.value)));
    TEST_ASSERT_EQUAL_size_t(COLLECTION_HASHTABLE_INITIAL_BUCKETS,
                             ht.buckets_size);
    assert_contents(&ht, &again, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RewindTheIterationOnDestroy(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };
    int                   key     = 0;
    int                   value   = 0;

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_pair(&ht, &key, sizeof(key), &value,
                                             sizeof(value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));

    /* a cursor left pointing into a freed chain would be walked next time */
    assert_at_start(&ht);
}

void test_hashtable_should_RejectANullTablePointerOnDestroy(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, hashtable_destroy(NULL));
}

/* -- PUT ---------------------------------------------------------------- */

void test_hashtable_should_StoreACopyOfBothTheKeyAndTheValue(void)
{
    const int key   = 0x1234;
    const int value = 0x5678;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &value,
                                        sizeof(value)));

    TEST_ASSERT_EQUAL_size_t(1, ht.size);
    assert_bookkeeping(&ht);

    /* exactly one pair, wherever the hash put it */
    struct hashtable_pair *stored = NULL;

    for (size_t i = 0; i < ht.buckets_size; i++)
    {
        if (ht.buckets[i] == NULL) continue;

        TEST_ASSERT_NULL(stored); /* a second bucket for a single pair */
        stored = ht.buckets[i];
        TEST_ASSERT_NULL(stored->next);
    }

    TEST_ASSERT_NOT_NULL(stored);
    TEST_ASSERT_EQUAL_size_t(sizeof(key), stored->key_size);
    TEST_ASSERT_EQUAL_size_t(sizeof(value), stored->value_size);
    TEST_ASSERT_EQUAL_MEMORY(&key, stored->key, sizeof(key));
    TEST_ASSERT_EQUAL_MEMORY(&value, stored->value, sizeof(value));
    /* copies of the caller's objects, not aliases of them */
    TEST_ASSERT_NOT_EQUAL_PTR(&key, stored->key);
    TEST_ASSERT_NOT_EQUAL_PTR(&value, stored->value);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReplaceTheValueWhenTheKeyIsAlreadyThere(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };
    const int             again   = 99;
    int                   out     = 0;

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &pairs[0].key,
                                        sizeof(pairs[0].key), &again,
                                        sizeof(again)));

    /* one key, one pair: replacing is not inserting */
    TEST_ASSERT_EQUAL_size_t(2, ht.size);
    assert_bookkeeping(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, &pairs[0].key,
                                        sizeof(pairs[0].key), &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_INT(again, out);

    /* the other pair was left alone */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, &pairs[1].key,
                                        sizeof(pairs[1].key), &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_INT(pairs[1].value, out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReplaceAValueWithOneOfADifferentSize(void)
{
    const int       key   = 5;
    const int       small = 10;
    const long long big   = 0x0BADF00DCAFEBABEll;
    long long       out   = 0;
    size_t          size  = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &small,
                                        sizeof(small)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &big,
                                        sizeof(big)));

    TEST_ASSERT_EQUAL_size_t(1, ht.size);

    /* the stored size followed the new value, so a get sized for the old one
       is now the mismatch */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_value_size(&ht, &key, sizeof(key),
                                                   &size));
    TEST_ASSERT_EQUAL_size_t(sizeof(big), size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, &key, sizeof(key), &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_HEX64(big, out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_KeepTheStoredKeyWhenReplacingAValue(void)
{
    const int key   = 5;
    const int first = 10;
    const int again = 20;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, hash_always_zero,
                                                        NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &first,
                                        sizeof(first)));

    struct hashtable_pair *pair       = ht.buckets[0];
    void                  *key_before = pair->key;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &again,
                                        sizeof(again)));

    /* the same pair, holding the same key it already had: only the value
       side of it was touched */
    TEST_ASSERT_EQUAL_PTR(pair, ht.buckets[0]);
    TEST_ASSERT_EQUAL_PTR(key_before, pair->key);
    TEST_ASSERT_EQUAL_MEMORY(&again, pair->value, sizeof(again));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ChainKeysThatLandInTheSameBucket(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };

    /* every key hashes to bucket 0, so nothing here is decided by luck */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_init(&ht, hash_always_zero, NULL));

    for (size_t i = 0; i < 3; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, &pairs[i].key,
                                            sizeof(pairs[i].key),
                                            &pairs[i].value,
                                            sizeof(pairs[i].value)));

    TEST_ASSERT_EQUAL_size_t(3, chain_length(&ht, 0));

    /* colliding keys are still three separate pairs, each with its own value */
    assert_contents(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_TellApartKeysOfDifferentSizesWithTheSameBytes(void)
{
    /* the same leading byte, stored under two different key sizes */
    const uint8_t  narrow = 7;
    const uint32_t wide   = 7;
    const int      first  = 100;
    const int      second = 200;
    int            out    = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &narrow, sizeof(narrow), &first,
                                        sizeof(first)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &wide, sizeof(wide), &second,
                                        sizeof(second)));

    /* the byte comparison calls keys of different sizes different keys, so
       these are two pairs and not one replaced twice */
    TEST_ASSERT_EQUAL_size_t(2, ht.size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, &narrow, sizeof(narrow), &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_INT(first, out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, &wide, sizeof(wide), &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_INT(second, out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectNullPointersOnPut(void)
{
    const int key   = 1;
    const int value = 10;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_put(NULL, &key, sizeof(key), &value,
                                        sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_put(&ht, NULL, sizeof(key), &value,
                                        sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_put(&ht, &key, sizeof(key), NULL,
                                        sizeof(value)));
    /* the sizes are irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_put(&ht, NULL, 0, NULL, 0));

    assert_empty(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectAZeroSizeOnPut(void)
{
    const int key   = 1;
    const int value = 10;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_put(&ht, &key, 0, &value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_put(&ht, &key, sizeof(key), &value, 0));

    assert_empty(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReportEnomemWhenAPairCannotBeAllocated(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };
    const int             key     = 3;
    const int             value   = 30;

    fill(&ht, pairs, 2);

    /* One key, and one value, no allocator can serve. Asking for a huge
       *number* of pairs instead would put until the machine ran out of
       memory, which is not something a test suite should do to the machine
       running it. */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          hashtable_put(&ht, &key, SIZE_MAX, &value,
                                        sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          hashtable_put(&ht, &key, sizeof(key), &value,
                                        SIZE_MAX));

    /* a failed put leaves the table exactly as it was */
    assert_contents(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_KeepTheOldValueWhenAReplacementCannotBeAllocated(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };
    const int             value   = 99;

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          hashtable_put(&ht, &pairs[0].key,
                                        sizeof(pairs[0].key), &value,
                                        SIZE_MAX));

    /* the key still has the value it had: the old one is not released until
       the new one is in hand */
    assert_contents(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_GrowTheBucketArrayWhenItFillsUp(void)
{
    /* 16 buckets grow past three quarters full, so the 13th pair is the one
       that tips it */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    for (int i = 0; i < 12; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, &i, sizeof(i), &i, sizeof(i)));

    TEST_ASSERT_EQUAL_size_t(COLLECTION_HASHTABLE_INITIAL_BUCKETS,
                             ht.buckets_size);

    const int tipping = 12;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &tipping, sizeof(tipping),
                                        &tipping, sizeof(tipping)));

    TEST_ASSERT_EQUAL_size_t(COLLECTION_HASHTABLE_INITIAL_BUCKETS * 2,
                             ht.buckets_size);

    /* the pairs were rehashed into the new array, not lost or duplicated */
    TEST_ASSERT_EQUAL_size_t(13, ht.size);
    assert_bookkeeping(&ht);

    for (int i = 0; i < 13; i++)
    {
        int out = 0;

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_get(&ht, &i, sizeof(i), &out,
                                            sizeof(out)));
        TEST_ASSERT_EQUAL_INT(i, out);
    }

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_NotGrowWhenAValueIsOnlyReplaced(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    for (int i = 0; i < 12; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, &i, sizeof(i), &i, sizeof(i)));

    /* replacing values, over and over, never adds a pair, so the load factor
       does not move and neither does the array */
    for (int round = 0; round < 5; round++)
        for (int i = 0; i < 12; i++)
        {
            const int value = i + round;

            TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                                  hashtable_put(&ht, &i, sizeof(i), &value,
                                                sizeof(value)));
        }

    TEST_ASSERT_EQUAL_size_t(12, ht.size);
    TEST_ASSERT_EQUAL_size_t(COLLECTION_HASHTABLE_INITIAL_BUCKETS,
                             ht.buckets_size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RewindTheIterationOnPut(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };
    const struct int_pair late    = { 4, 40 };
    int                   key     = 0;
    int                   value   = 0;

    fill(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_pair(&ht, &key, sizeof(key), &value,
                                             sizeof(value)));
    TEST_ASSERT_FALSE(ht.cursor_bucket == 0 && ht.cursor_pair == NULL);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &late.key, sizeof(late.key),
                                        &late.value, sizeof(late.value)));

    /* a put can rehash everything, so the walk starts over rather than
       skipping or repeating pairs */
    assert_at_start(&ht);

    const struct int_pair expected[] = { { 1, 10 }, { 2, 20 }, { 3, 30 },
                                         { 4, 40 } };

    assert_walk_yields(&ht, expected, 4);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

/* -- GET ---------------------------------------------------------------- */

void test_hashtable_should_CopyOutTheValueStoredUnderAKey(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };

    fill(&ht, pairs, 3);

    for (size_t i = 0; i < 3; i++)
    {
        int out = 0;

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_get(&ht, &pairs[i].key,
                                            sizeof(pairs[i].key), &out,
                                            sizeof(out)));
        TEST_ASSERT_EQUAL_INT(pairs[i].value, out);
    }

    /* getting is not taking: the table still holds everything */
    assert_contents(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_CopyOutExactlyTheStoredValue(void)
{
    const int key   = 1;
    const int value = 0x0BADF00D;
    /* a guard right behind the destination: a get that wrote more than the
       value is worth would trample it */
    int       out[2] = { 0, 0x5A5A5A5A };

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &value,
                                        sizeof(value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, &key, sizeof(key), &out[0],
                                        sizeof(out[0])));

    TEST_ASSERT_EQUAL_HEX32(value, out[0]);
    TEST_ASSERT_EQUAL_HEX32(0x5A5A5A5A, out[1]);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_FindAKeyThatCollidesWithAnother(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 },
                                      { 4, 40 } };

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_init(&ht, hash_always_zero, NULL));

    for (size_t i = 0; i < 4; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, &pairs[i].key,
                                            sizeof(pairs[i].key),
                                            &pairs[i].value,
                                            sizeof(pairs[i].value)));

    /* every key is in the same chain, so a get has to walk past the wrong
       ones to reach the right one */
    assert_contents(&ht, pairs, 4);

    /* and a key that hashes there but is not in the chain is still missing */
    const int absent = 99;
    int       out    = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_get(&ht, &absent, sizeof(absent), &out,
                                        sizeof(out)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReportNotfoundForAKeyItDoesNotHold(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };
    const int             absent  = 3;
    int                   out     = 0x1234;

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_get(&ht, &absent, sizeof(absent), &out,
                                        sizeof(out)));

    /* the destination was never written */
    TEST_ASSERT_EQUAL_INT(0x1234, out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReportNotfoundWhenGettingFromAnEmptyTable(void)
{
    const int key = 1;
    int       out = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_get(&ht, &key, sizeof(key), &out,
                                        sizeof(out)));

    TEST_ASSERT_EQUAL_INT(0x1234, out);
    assert_empty(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectAValueSizeThatDoesNotMatchOnGet(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };
    /* wide enough that a mismatched copy would still fit, so only the size
       check can stop it */
    long long             out     = 0;

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_get(&ht, &pairs[0].key,
                                        sizeof(pairs[0].key), &out,
                                        sizeof(out)));

    /* the destination was never written */
    TEST_ASSERT_EQUAL_HEX64(0, out);

    /* the right size still works, so the table was not left disturbed */
    int right = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, &pairs[0].key,
                                        sizeof(pairs[0].key), &right,
                                        sizeof(right)));
    TEST_ASSERT_EQUAL_INT(pairs[0].value, right);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectNullPointersOnGet(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };
    int                   out     = 0x1234;

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_get(NULL, &pairs[0].key,
                                        sizeof(pairs[0].key), &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_get(&ht, NULL, sizeof(pairs[0].key), &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_get(&ht, &pairs[0].key,
                                        sizeof(pairs[0].key), NULL,
                                        sizeof(out)));
    /* the sizes are irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_get(&ht, NULL, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(0x1234, out);
    assert_contents(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectAZeroSizeOnGet(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };
    int                   out     = 0x1234;

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_get(&ht, &pairs[0].key, 0, &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_get(&ht, &pairs[0].key,
                                        sizeof(pairs[0].key), &out, 0));

    TEST_ASSERT_EQUAL_INT(0x1234, out);
    assert_contents(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

/* -- GET_VALUE_SIZE ----------------------------------------------------- */

void test_hashtable_should_ReportTheSizeAValueWasStoredWith(void)
{
    const int       key   = 1;
    const long long value = 0x0BADF00DCAFEBABEll;
    size_t          size  = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &value,
                                        sizeof(value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_value_size(&ht, &key, sizeof(key),
                                                   &size));
    TEST_ASSERT_EQUAL_size_t(sizeof(value), size);

    /* which is exactly what a get sized by it needs */
    long long out = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, &key, sizeof(key), &out, size));
    TEST_ASSERT_EQUAL_HEX64(value, out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReportNotfoundWhenSizingAValueItDoesNotHold(void)
{
    const struct int_pair pairs[]  = { { 1, 10 } };
    const int             absent   = 2;
    size_t                size     = 0x1234;

    fill(&ht, pairs, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_get_value_size(&ht, &absent,
                                                   sizeof(absent), &size));

    /* the out parameter was never written */
    TEST_ASSERT_EQUAL_size_t(0x1234, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectBadArgumentsOnGetValueSize(void)
{
    const struct int_pair pairs[] = { { 1, 10 } };
    size_t                size    = 0x1234;

    fill(&ht, pairs, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_get_value_size(NULL, &pairs[0].key,
                                                   sizeof(pairs[0].key),
                                                   &size));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_get_value_size(&ht, NULL,
                                                   sizeof(pairs[0].key),
                                                   &size));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_get_value_size(&ht, &pairs[0].key,
                                                   sizeof(pairs[0].key),
                                                   NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_get_value_size(&ht, &pairs[0].key, 0,
                                                   &size));

    TEST_ASSERT_EQUAL_size_t(0x1234, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

/* -- CONTAINS ----------------------------------------------------------- */

void test_hashtable_should_ReportContainsOnlyForKeysItHolds(void)
{
    const struct int_pair pairs[]  = { { 1, 10 }, { 2, 20 } };
    const int             absent   = 3;
    bool                  contains = false;

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_contains(&ht, &pairs[0].key,
                                             sizeof(pairs[0].key), &contains));
    TEST_ASSERT_TRUE(contains);

    /* a key that is not there is an answer, not a failure */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_contains(&ht, &absent, sizeof(absent),
                                             &contains));
    TEST_ASSERT_FALSE(contains);

    /* and it follows a remove */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_remove(&ht, &pairs[0].key,
                                           sizeof(pairs[0].key)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_contains(&ht, &pairs[0].key,
                                             sizeof(pairs[0].key), &contains));
    TEST_ASSERT_FALSE(contains);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReportNotContainsOnAnEmptyTable(void)
{
    const int key      = 1;
    bool      contains = true;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_contains(&ht, &key, sizeof(key),
                                             &contains));
    TEST_ASSERT_FALSE(contains);

    assert_empty(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectBadArgumentsOnContains(void)
{
    const struct int_pair pairs[]  = { { 1, 10 } };
    bool                  contains = true;

    fill(&ht, pairs, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_contains(NULL, &pairs[0].key,
                                             sizeof(pairs[0].key), &contains));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_contains(&ht, NULL,
                                             sizeof(pairs[0].key), &contains));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_contains(&ht, &pairs[0].key,
                                             sizeof(pairs[0].key), NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_contains(&ht, &pairs[0].key, 0,
                                             &contains));

    /* the out parameter was never written */
    TEST_ASSERT_TRUE(contains);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

/* -- REMOVE ------------------------------------------------------------- */

void test_hashtable_should_RemoveThePairStoredUnderAKey(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };
    int                   out     = 0x1234;

    fill(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_remove(&ht, &pairs[1].key,
                                           sizeof(pairs[1].key)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_get(&ht, &pairs[1].key,
                                        sizeof(pairs[1].key), &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_INT(0x1234, out);

    /* the pairs around it were left alone */
    const struct int_pair left[] = { { 1, 10 }, { 3, 30 } };

    assert_contents(&ht, left, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RelinkTheChainWhereverThePairSatInIt(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };

    /* one bucket, one chain of three, so each removal is a known position in
       it rather than whichever one the hash happened to pick */
    for (size_t target = 0; target < 3; target++)
    {
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_init(&ht, hash_always_zero, NULL));

        for (size_t i = 0; i < 3; i++)
            TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                                  hashtable_put(&ht, &pairs[i].key,
                                                sizeof(pairs[i].key),
                                                &pairs[i].value,
                                                sizeof(pairs[i].value)));

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_remove(&ht, &pairs[target].key,
                                               sizeof(pairs[target].key)));

        TEST_ASSERT_EQUAL_size_t(2, chain_length(&ht, 0));
        TEST_ASSERT_EQUAL_size_t(2, ht.size);
        assert_bookkeeping(&ht);

        /* the two that were not removed are both still reachable, which a
           chain relinked past the wrong pair would break */
        for (size_t i = 0; i < 3; i++)
        {
            int out = 0;

            if (i == target)
            {
                TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                                      hashtable_get(&ht, &pairs[i].key,
                                                    sizeof(pairs[i].key),
                                                    &out, sizeof(out)));
                continue;
            }

            TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                                  hashtable_get(&ht, &pairs[i].key,
                                                sizeof(pairs[i].key), &out,
                                                sizeof(out)));
            TEST_ASSERT_EQUAL_INT(pairs[i].value, out);
        }

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
    }
}

void test_hashtable_should_ClearTheBucketWhenItsLastPairIsRemoved(void)
{
    const int key   = 1;
    const int value = 10;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_init(&ht, hash_always_zero, NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &value,
                                        sizeof(value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_remove(&ht, &key, sizeof(key)));

    /* a stale head would be dereferenced by the next lookup in this bucket */
    TEST_ASSERT_NULL(ht.buckets[0]);
    TEST_ASSERT_EQUAL_size_t(0, ht.size);
    assert_bookkeeping(&ht);

    /* and the table takes pairs again */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &value,
                                        sizeof(value)));
    TEST_ASSERT_EQUAL_size_t(1, chain_length(&ht, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReportNotfoundWhenRemovingAKeyItDoesNotHold(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };
    const int             absent  = 3;

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_remove(&ht, &absent, sizeof(absent)));

    /* a failed remove takes nothing with it */
    assert_contents(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReportNotfoundWhenRemovingFromAnEmptyTable(void)
{
    const int key = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_remove(&ht, &key, sizeof(key)));

    assert_empty(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectBadArgumentsOnRemove(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_remove(NULL, &pairs[0].key,
                                           sizeof(pairs[0].key)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_remove(&ht, NULL, sizeof(pairs[0].key)));
    /* the size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, hashtable_remove(&ht, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_remove(&ht, &pairs[0].key, 0));

    assert_contents(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ShrinkTheBucketArrayAsItEmpties(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    for (int i = 0; i < 13; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, &i, sizeof(i), &i, sizeof(i)));

    TEST_ASSERT_EQUAL_size_t(COLLECTION_HASHTABLE_INITIAL_BUCKETS * 2,
                             ht.buckets_size);

    /* 32 buckets shrink below a quarter full, so 8 pairs is still not few
       enough. The gap between that and the 25 it would take to grow again is
       what keeps a table hovering at a threshold from resizing on every call */
    for (int i = 0; i < 5; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_remove(&ht, &i, sizeof(i)));

    TEST_ASSERT_EQUAL_size_t(8, ht.size);
    TEST_ASSERT_EQUAL_size_t(COLLECTION_HASHTABLE_INITIAL_BUCKETS * 2,
                             ht.buckets_size);

    const int tipping = 5;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_remove(&ht, &tipping, sizeof(tipping)));

    TEST_ASSERT_EQUAL_size_t(COLLECTION_HASHTABLE_INITIAL_BUCKETS,
                             ht.buckets_size);

    /* the survivors were rehashed into the smaller array, not lost */
    TEST_ASSERT_EQUAL_size_t(7, ht.size);
    assert_bookkeeping(&ht);

    for (int i = 6; i < 13; i++)
    {
        int out = 0;

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_get(&ht, &i, sizeof(i), &out,
                                            sizeof(out)));
        TEST_ASSERT_EQUAL_INT(i, out);
    }

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_NeverShrinkBelowTheInitialBucketCount(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    for (int i = 0; i < 20; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, &i, sizeof(i), &i, sizeof(i)));

    for (int i = 0; i < 20; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_remove(&ht, &i, sizeof(i)));

    /* emptied all the way down, and still holding the floor */
    assert_empty(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RewindTheIterationOnRemove(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };
    int                   key     = 0;
    int                   value   = 0;

    fill(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_pair(&ht, &key, sizeof(key), &value,
                                             sizeof(value)));
    TEST_ASSERT_FALSE(ht.cursor_bucket == 0 && ht.cursor_pair == NULL);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_remove(&ht, &pairs[0].key,
                                           sizeof(pairs[0].key)));

    /* the cursor could have been sitting on the pair that was just freed */
    assert_at_start(&ht);

    const struct int_pair left[] = { { 2, 20 }, { 3, 30 } };

    assert_walk_yields(&ht, left, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_LeaveTheIterationAloneWhenARemoveFindsNothing(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };
    const int             absent  = 4;
    int                   key     = 0;
    int                   value   = 0;

    fill(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_pair(&ht, &key, sizeof(key), &value,
                                             sizeof(value)));

    const size_t                 bucket_before = ht.cursor_bucket;
    const struct hashtable_pair *pair_before   = ht.cursor_pair;

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_remove(&ht, &absent, sizeof(absent)));

    /* a call that changed nothing has no reason to disturb a walk in progress */
    TEST_ASSERT_EQUAL_size_t(bucket_before, ht.cursor_bucket);
    TEST_ASSERT_EQUAL_PTR(pair_before, ht.cursor_pair);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

/* -- GET_PAIR ----------------------------------------------------------- */

void test_hashtable_should_HandBackEveryPairExactlyOnce(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 },
                                      { 4, 40 }, { 5, 50 } };

    fill(&ht, pairs, 5);

    assert_walk_yields(&ht, pairs, 5);

    /* the pairs were copied out, not taken out */
    assert_contents(&ht, pairs, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_WalkAWholeChainOfCollidingPairs(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 },
                                      { 4, 40 } };

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_init(&ht, hash_always_zero, NULL));

    for (size_t i = 0; i < 4; i++)
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, &pairs[i].key,
                                            sizeof(pairs[i].key),
                                            &pairs[i].value,
                                            sizeof(pairs[i].value)));

    /* everything sits in one bucket, so the walk has to follow a chain
       rather than only step from bucket to bucket */
    assert_walk_yields(&ht, pairs, 4);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_WalkATableThatHasGrown(void)
{
    struct int_pair pairs[40];

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    for (int i = 0; i < 40; i++)
    {
        pairs[i].key   = i;
        pairs[i].value = i * 10;

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, &pairs[i].key,
                                            sizeof(pairs[i].key),
                                            &pairs[i].value,
                                            sizeof(pairs[i].value)));
    }

    /* several resizes in, the walk still reaches every pair once: no bucket
       is skipped and none is visited twice */
    TEST_ASSERT_TRUE(ht.buckets_size > COLLECTION_HASHTABLE_INITIAL_BUCKETS);
    assert_walk_yields(&ht, pairs, 40);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReportNotfoundWhenWalkingAnEmptyTable(void)
{
    int key   = 0x1234;
    int value = 0x5678;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_get_pair(&ht, &key, sizeof(key), &value,
                                             sizeof(value)));

    /* neither destination was written */
    TEST_ASSERT_EQUAL_INT(0x1234, key);
    TEST_ASSERT_EQUAL_INT(0x5678, value);
    assert_empty(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_StartOverAfterARewind(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };
    int                   first_key   = 0;
    int                   first_value = 0;
    int                   key         = 0;
    int                   value       = 0;

    fill(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_pair(&ht, &first_key,
                                             sizeof(first_key), &first_value,
                                             sizeof(first_value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_pair(&ht, &key, sizeof(key), &value,
                                             sizeof(value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_rewind(&ht));
    assert_at_start(&ht);

    /* the same table, unchanged, walks in the same order again */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_pair(&ht, &key, sizeof(key), &value,
                                             sizeof(value)));
    TEST_ASSERT_EQUAL_INT(first_key, key);
    TEST_ASSERT_EQUAL_INT(first_value, value);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RewindATableItNeverWalked(void)
{
    const struct int_pair pairs[] = { { 1, 10 } };

    fill(&ht, pairs, 1);

    /* already at the start, and asked to go there anyway */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_rewind(&ht));
    assert_at_start(&ht);
    assert_contents(&ht, pairs, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectANullTablePointerOnRewind(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, hashtable_rewind(NULL));
}

void test_hashtable_should_NotAdvanceTheCursorWhenAPairDoesNotFit(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };
    /* wide enough that a mismatched copy would still fit, so only the size
       check can stop it */
    long long             wide    = 0;
    int                   key     = 0;
    int                   value   = 0;

    fill(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_get_pair(&ht, &wide, sizeof(wide), &value,
                                             sizeof(value)));
    assert_at_start(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_get_pair(&ht, &key, sizeof(key), &wide,
                                             sizeof(wide)));
    assert_at_start(&ht);

    /* nothing was written and nothing was skipped, so the whole table is
       still there to be walked */
    TEST_ASSERT_EQUAL_HEX64(0, wide);
    assert_walk_yields(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectBadArgumentsOnGetPairWithoutAdvancing(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 } };
    int                   key     = 0x1234;
    int                   value   = 0x5678;

    fill(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_get_pair(NULL, &key, sizeof(key), &value,
                                             sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_get_pair(&ht, NULL, sizeof(key), &value,
                                             sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_get_pair(&ht, &key, sizeof(key), NULL,
                                             sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_get_pair(&ht, &key, 0, &value,
                                             sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          hashtable_get_pair(&ht, &key, sizeof(key), &value,
                                             0));

    TEST_ASSERT_EQUAL_INT(0x1234, key);
    TEST_ASSERT_EQUAL_INT(0x5678, value);
    assert_at_start(&ht);
    assert_walk_yields(&ht, pairs, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_WalkPairsOfDifferentSizesWithTheSizesItReports(void)
{
    const int       small_key   = 1;
    const int       small_value = 10;
    const long long big_key     = 0x0BADF00DCAFEBABEll;
    const long long big_value   = 0x1122334455667788ll;
    size_t          walked      = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &small_key, sizeof(small_key),
                                        &small_value, sizeof(small_value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &big_key, sizeof(big_key),
                                        &big_value, sizeof(big_value)));

    /* the caller knows neither which pair comes first nor how wide it is, so
       it asks before every step */
    for (;;)
    {
        size_t key_size   = 0;
        size_t value_size = 0;

        if (hashtable_peek_pair_size(&ht, &key_size, &value_size) ==
            COLLECTION_ENOTFOUND)
            break;

        long long key   = 0;
        long long value = 0;

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_get_pair(&ht, &key, key_size, &value,
                                                 value_size));

        if (key_size == sizeof(small_key))
        {
            TEST_ASSERT_EQUAL_size_t(sizeof(small_value), value_size);
            TEST_ASSERT_EQUAL_INT(small_key, (int)key);
            TEST_ASSERT_EQUAL_INT(small_value, (int)value);
        }
        else
        {
            TEST_ASSERT_EQUAL_size_t(sizeof(big_key), key_size);
            TEST_ASSERT_EQUAL_size_t(sizeof(big_value), value_size);
            TEST_ASSERT_EQUAL_HEX64(big_key, key);
            TEST_ASSERT_EQUAL_HEX64(big_value, value);
        }

        walked++;
    }

    TEST_ASSERT_EQUAL_size_t(2, walked);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

/* -- PEEK_PAIR_SIZE ----------------------------------------------------- */

void test_hashtable_should_ReportTheSizesOfTheNextPairWithoutAdvancing(void)
{
    const long long key   = 0x0BADF00DCAFEBABEll;
    const int       value = 10;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &value,
                                        sizeof(value)));

    size_t key_size   = 0;
    size_t value_size = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_peek_pair_size(&ht, &key_size,
                                                   &value_size));
    TEST_ASSERT_EQUAL_size_t(sizeof(key), key_size);
    TEST_ASSERT_EQUAL_size_t(sizeof(value), value_size);

    /* asking twice gives the same answer: peeking does not consume */
    assert_at_start(&ht);
    key_size   = 0;
    value_size = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_peek_pair_size(&ht, &key_size,
                                                   &value_size));
    TEST_ASSERT_EQUAL_size_t(sizeof(key), key_size);
    TEST_ASSERT_EQUAL_size_t(sizeof(value), value_size);

    /* and it describes the pair the next get_pair really hands back */
    long long out_key   = 0;
    int       out_value = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_pair(&ht, &out_key, key_size,
                                             &out_value, value_size));
    TEST_ASSERT_EQUAL_HEX64(key, out_key);
    TEST_ASSERT_EQUAL_INT(value, out_value);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReportNotfoundWhenPeekingPastTheLastPair(void)
{
    const struct int_pair pairs[] = { { 1, 10 } };
    int                   key     = 0;
    int                   value   = 0;
    size_t                key_size   = 0x1234;
    size_t                value_size = 0x5678;

    fill(&ht, pairs, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get_pair(&ht, &key, sizeof(key), &value,
                                             sizeof(value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_peek_pair_size(&ht, &key_size,
                                                   &value_size));

    /* the out parameters were never written */
    TEST_ASSERT_EQUAL_size_t(0x1234, key_size);
    TEST_ASSERT_EQUAL_size_t(0x5678, value_size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_ReportNotfoundWhenPeekingAnEmptyTable(void)
{
    size_t key_size   = 0x1234;
    size_t value_size = 0x5678;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          hashtable_peek_pair_size(&ht, &key_size,
                                                   &value_size));

    TEST_ASSERT_EQUAL_size_t(0x1234, key_size);
    TEST_ASSERT_EQUAL_size_t(0x5678, value_size);
    assert_empty(&ht);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectNullPointersOnPeekPairSize(void)
{
    const struct int_pair pairs[]    = { { 1, 10 } };
    size_t                key_size   = 0x1234;
    size_t                value_size = 0x5678;

    fill(&ht, pairs, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_peek_pair_size(NULL, &key_size,
                                                   &value_size));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_peek_pair_size(&ht, NULL, &value_size));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          hashtable_peek_pair_size(&ht, &key_size, NULL));

    TEST_ASSERT_EQUAL_size_t(0x1234, key_size);
    TEST_ASSERT_EQUAL_size_t(0x5678, value_size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

/* -- GET_SIZE ----------------------------------------------------------- */

void test_hashtable_should_ReportTheNumberOfPairsItHolds(void)
{
    const struct int_pair pairs[] = { { 1, 10 }, { 2, 20 }, { 3, 30 } };
    size_t                size    = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_get_size(&ht, &size));
    TEST_ASSERT_EQUAL_size_t(0, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
    fill(&ht, pairs, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_get_size(&ht, &size));
    TEST_ASSERT_EQUAL_size_t(3, size);

    /* a replaced value is not a new pair */
    const int again = 99;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &pairs[0].key,
                                        sizeof(pairs[0].key), &again,
                                        sizeof(again)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_get_size(&ht, &size));
    TEST_ASSERT_EQUAL_size_t(3, size);

    /* and it follows the table down as well as up */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_remove(&ht, &pairs[0].key,
                                           sizeof(pairs[0].key)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_get_size(&ht, &size));
    TEST_ASSERT_EQUAL_size_t(2, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_get_size(&ht, &size));
    TEST_ASSERT_EQUAL_size_t(0, size);
}

void test_hashtable_should_RejectNullPointersOnGetSize(void)
{
    const struct int_pair pairs[] = { { 1, 10 } };
    size_t                size    = 0x1234;

    fill(&ht, pairs, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, hashtable_get_size(NULL, &size));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, hashtable_get_size(&ht, NULL));

    /* the out parameter was never written */
    TEST_ASSERT_EQUAL_size_t(0x1234, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

/* -- IS_EMPTY ----------------------------------------------------------- */

void test_hashtable_should_ReportEmptyOnlyWhileItHoldsNoPairs(void)
{
    const int key   = 1;
    const int value = 10;
    bool      empty = false;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_is_empty(&ht, &empty));
    TEST_ASSERT_TRUE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, &key, sizeof(key), &value,
                                        sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_is_empty(&ht, &empty));
    TEST_ASSERT_FALSE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_remove(&ht, &key, sizeof(key)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_is_empty(&ht, &empty));
    TEST_ASSERT_TRUE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_RejectNullPointersOnIsEmpty(void)
{
    const struct int_pair pairs[] = { { 1, 10 } };
    bool                  empty   = true;

    fill(&ht, pairs, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, hashtable_is_empty(NULL, &empty));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, hashtable_is_empty(&ht, NULL));

    /* the out parameter was never written */
    TEST_ASSERT_TRUE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

/* -- HASH_BYTES --------------------------------------------------------- */

void test_hashtable_should_HashTheSameBytesToTheSameValue(void)
{
    const int first  = 0x0BADF00D;
    const int second = 0x0BADF00D;

    /* two objects, same bytes: the hash is of the contents, not the address */
    TEST_ASSERT_EQUAL_size_t(hashtable_hash_bytes(&first, sizeof(first)),
                             hashtable_hash_bytes(&second, sizeof(second)));

    /* and it is stable across calls */
    TEST_ASSERT_EQUAL_size_t(hashtable_hash_bytes(&first, sizeof(first)),
                             hashtable_hash_bytes(&first, sizeof(first)));
}

void test_hashtable_should_HashTheSameBytesUnderDifferentSizesDifferently(void)
{
    const uint32_t value = 0;

    /* one byte of it against four: a key is its bytes and how many of them */
    TEST_ASSERT_NOT_EQUAL_size_t(hashtable_hash_bytes(&value, 1),
                                 hashtable_hash_bytes(&value, sizeof(value)));
}

void test_hashtable_should_HashNothingToZero(void)
{
    const int value = 1;

    /* a key that is not there at all, rather than one that happens to hash
       to something */
    TEST_ASSERT_EQUAL_size_t(0, hashtable_hash_bytes(NULL, sizeof(value)));
    TEST_ASSERT_EQUAL_size_t(0, hashtable_hash_bytes(&value, 0));
    TEST_ASSERT_EQUAL_size_t(0, hashtable_hash_string(NULL, sizeof(value)));
    TEST_ASSERT_EQUAL_size_t(0, hashtable_hash_string(&value, 0));
}

void test_hashtable_should_ScatterKeysThatDifferByOneBit(void)
{
    const uint32_t base = 0x12345678;

    /* flipping any single bit has to move the hash. A hash that only folded
       bytes together would leave neighbours next to each other, which is
       what fills one bucket and leaves the rest empty */
    for (unsigned bit = 0; bit < 32; bit++)
    {
        const uint32_t flipped = base ^ (1u << bit);

        TEST_ASSERT_NOT_EQUAL_size_t(hashtable_hash_bytes(&base, sizeof(base)),
                                     hashtable_hash_bytes(&flipped,
                                                          sizeof(flipped)));
    }
}

void test_hashtable_should_SpreadConsecutiveIntegersAcrossBuckets(void)
{
    /* the classic bad case: 1000 keys that differ only in their low bits,
       dropped into 1024 buckets the way the table does it */
    enum { KEYS = 1000, BUCKETS = 1024 };

    static size_t counts[BUCKETS];

    memset(counts, 0, sizeof(counts));

    for (int i = 0; i < KEYS; i++)
        counts[hashtable_hash_bytes(&i, sizeof(i)) & (BUCKETS - 1)]++;

    size_t used    = 0;
    size_t longest = 0;

    for (size_t i = 0; i < BUCKETS; i++)
    {
        if (counts[i] > 0) used++;
        if (counts[i] > longest) longest = counts[i];
    }

    /* throwing 1000 keys into 1024 buckets at random fills about 638 of
       them, so a hash that spreads properly lands near there. Well under
       that means the keys are piling up */
    TEST_ASSERT_TRUE_MESSAGE(used >= 560, "keys are clustering in few buckets");
    /* and no single lookup degenerates into a long walk */
    TEST_ASSERT_TRUE_MESSAGE(longest <= 8, "one bucket took far too many keys");
}

void test_hashtable_should_SpreadKeysWhoseLowBitsAreAlwaysZero(void)
{
    /* what pointers and struct sized offsets look like: every key a multiple
       of 16, so the bottom four bits never change. Masking an unmixed hash
       here would use 64 buckets out of 1024 */
    enum { KEYS = 1000, BUCKETS = 1024 };

    static size_t counts[BUCKETS];

    memset(counts, 0, sizeof(counts));

    for (int i = 0; i < KEYS; i++)
    {
        const int key = i * 16;

        counts[hashtable_hash_bytes(&key, sizeof(key)) & (BUCKETS - 1)]++;
    }

    size_t used = 0;

    for (size_t i = 0; i < BUCKETS; i++)
        if (counts[i] > 0) used++;

    TEST_ASSERT_TRUE_MESSAGE(used >= 560, "keys are clustering in few buckets");
}

void test_hashtable_should_SpreadStringsAcrossBuckets(void)
{
    enum { KEYS = 1000, BUCKETS = 1024 };

    static size_t counts[BUCKETS];

    memset(counts, 0, sizeof(counts));

    /* keys that share a long prefix and differ only at the end, which is how
       most real string keys look */
    for (int i = 0; i < KEYS; i++)
    {
        char key[32];

        snprintf(key, sizeof(key), "collection/key/%d", i);
        counts[hashtable_hash_string(key, sizeof(key)) & (BUCKETS - 1)]++;
    }

    size_t used    = 0;
    size_t longest = 0;

    for (size_t i = 0; i < BUCKETS; i++)
    {
        if (counts[i] > 0) used++;
        if (counts[i] > longest) longest = counts[i];
    }

    TEST_ASSERT_TRUE_MESSAGE(used >= 560, "keys are clustering in few buckets");
    TEST_ASSERT_TRUE_MESSAGE(longest <= 8, "one bucket took far too many keys");
}

/* -- HASH_STRING -------------------------------------------------------- */

void test_hashtable_should_HashAStringTheSameWithOrWithoutItsTerminator(void)
{
    const char *text = "collection";

    /* which is the whole reason this hash exists: strlen() and strlen() + 1
       have to agree, or a key stored one way is never found the other */
    TEST_ASSERT_EQUAL_size_t(hashtable_hash_string(text, strlen(text)),
                             hashtable_hash_string(text, strlen(text) + 1));

    /* trailing garbage past the NUL is not part of the key either */
    const char padded[] = { 'c', 'o', 'l', 'l', 'e', 'c', 't', 'i', 'o', 'n',
                            '\0', 'X', 'Y', 'Z' };

    TEST_ASSERT_EQUAL_size_t(hashtable_hash_string(text, strlen(text)),
                             hashtable_hash_string(padded, sizeof(padded)));
}

void test_hashtable_should_StopHashingAStringAtTheSizeItWasGiven(void)
{
    const char text[] = "collection";

    /* a buffer with no NUL in it at all is the string it spells, and nothing
       past the size is read */
    const char unterminated[3] = { 'c', 'o', 'l' };

    TEST_ASSERT_EQUAL_size_t(hashtable_hash_string("col", 3),
                             hashtable_hash_string(unterminated,
                                                   sizeof(unterminated)));

    /* and cutting a longer string short really does change the key */
    TEST_ASSERT_NOT_EQUAL_size_t(hashtable_hash_string(text, sizeof(text)),
                                 hashtable_hash_string(text, 3));
}

void test_hashtable_should_HashDifferentStringsDifferently(void)
{
    const char *first  = "collection";
    const char *second = "collectioo";
    const char *third  = "noitcelloc";

    TEST_ASSERT_NOT_EQUAL_size_t(hashtable_hash_string(first, strlen(first)),
                                 hashtable_hash_string(second,
                                                       strlen(second)));
    /* the same bytes in another order are another key: order has to matter */
    TEST_ASSERT_NOT_EQUAL_size_t(hashtable_hash_string(first, strlen(first)),
                                 hashtable_hash_string(third, strlen(third)));
}

/* -- CMP_BYTES ---------------------------------------------------------- */

void test_hashtable_should_CallEqualBytesOfEqualSizeTheSameKey(void)
{
    const int first  = 0x0BADF00D;
    const int second = 0x0BADF00D;

    TEST_ASSERT_EQUAL_INT(0, hashtable_cmp_bytes(&first, sizeof(first),
                                                 &second, sizeof(second)));
}

void test_hashtable_should_CallDifferentBytesDifferentKeys(void)
{
    const int first  = 1;
    const int second = 2;

    TEST_ASSERT_NOT_EQUAL_INT(0, hashtable_cmp_bytes(&first, sizeof(first),
                                                     &second,
                                                     sizeof(second)));
}

void test_hashtable_should_CallKeysOfDifferentSizesDifferentKeys(void)
{
    const uint32_t wide   = 0;
    const uint8_t  narrow = 0;

    /* even though the shorter one is a prefix of the longer one */
    TEST_ASSERT_NOT_EQUAL_INT(0, hashtable_cmp_bytes(&wide, sizeof(wide),
                                                     &narrow,
                                                     sizeof(narrow)));
}

/* -- CMP_STRING --------------------------------------------------------- */

void test_hashtable_should_CallAStringTheSameKeyWithOrWithoutItsTerminator(void)
{
    const char *text = "collection";

    TEST_ASSERT_EQUAL_INT(0, hashtable_cmp_string(text, strlen(text), text,
                                                  strlen(text) + 1));
}

void test_hashtable_should_CallDifferentStringsDifferentKeys(void)
{
    const char *first  = "collection";
    const char *second = "collections";

    /* a prefix is not the string it is a prefix of */
    TEST_ASSERT_NOT_EQUAL_INT(0, hashtable_cmp_string(first, strlen(first),
                                                      second,
                                                      strlen(second)));
    TEST_ASSERT_NOT_EQUAL_INT(0, hashtable_cmp_string("abc", 3, "abd", 3));
}

void test_hashtable_should_StopComparingStringsAtTheSizeTheyWereGiven(void)
{
    const char unterminated[3] = { 'c', 'o', 'l' };

    /* no read runs past the size a key was stored with, so a buffer with no
       NUL is the string it spells and nothing more */
    TEST_ASSERT_EQUAL_INT(0, hashtable_cmp_string(unterminated,
                                                  sizeof(unterminated), "col",
                                                  4));
    TEST_ASSERT_NOT_EQUAL_INT(0, hashtable_cmp_string(unterminated,
                                                      sizeof(unterminated),
                                                      "coll", 5));
}

/* -- STRING KEYS END TO END --------------------------------------------- */

void test_hashtable_should_HoldStringKeysStoredWithOrWithoutTheirTerminator(void)
{
    const char *key   = "answer";
    const int   value = 42;
    int         out   = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_init(&ht, hashtable_hash_string,
                                         hashtable_cmp_string));

    /* stored counting the terminator */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, key, strlen(key) + 1, &value,
                                        sizeof(value)));

    /* found without counting it, because the hash and the comparison draw
       the line at the same place */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, key, strlen(key), &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_INT(value, out);

    /* and it is the same key, so putting it the other way round replaces */
    const int again = 43;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_put(&ht, key, strlen(key), &again,
                                        sizeof(again)));
    TEST_ASSERT_EQUAL_size_t(1, ht.size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_get(&ht, key, strlen(key) + 1, &out,
                                        sizeof(out)));
    TEST_ASSERT_EQUAL_INT(again, out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_HoldManyStringKeysAtOnce(void)
{
    enum { KEYS = 50 };

    char key[16];

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          hashtable_init(&ht, hashtable_hash_string,
                                         hashtable_cmp_string));

    for (int i = 0; i < KEYS; i++)
    {
        snprintf(key, sizeof(key), "key%d", i);
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, key, strlen(key) + 1, &i,
                                            sizeof(i)));
    }

    TEST_ASSERT_EQUAL_size_t(KEYS, ht.size);
    assert_bookkeeping(&ht);

    for (int i = 0; i < KEYS; i++)
    {
        int out = 0;

        snprintf(key, sizeof(key), "key%d", i);
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_get(&ht, key, strlen(key) + 1, &out,
                                            sizeof(out)));
        TEST_ASSERT_EQUAL_INT(i, out);
    }

    /* the keys were copied in, so the one buffer they were all written
       through does not matter */
    memset(key, 0, sizeof(key));
    TEST_ASSERT_EQUAL_size_t(KEYS, ht.size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

/* -- MANY PAIRS --------------------------------------------------------- */

void test_hashtable_should_SurviveFillingAndEmptyingRepeatedly(void)
{
    enum { KEYS = 200 };

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    for (int round = 0; round < 3; round++)
    {
        for (int i = 0; i < KEYS; i++)
        {
            const int value = i + round;

            TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                                  hashtable_put(&ht, &i, sizeof(i), &value,
                                                sizeof(value)));
        }

        TEST_ASSERT_EQUAL_size_t(KEYS, ht.size);
        assert_bookkeeping(&ht);

        for (int i = 0; i < KEYS; i++)
        {
            int out = 0;

            TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                                  hashtable_get(&ht, &i, sizeof(i), &out,
                                                sizeof(out)));
            TEST_ASSERT_EQUAL_INT(i + round, out);
        }

        /* removing in the reverse order of insertion, so the shrinking
           happens on chains built in the other direction */
        for (int i = KEYS - 1; i >= 0; i--)
            TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                                  hashtable_remove(&ht, &i, sizeof(i)));

        assert_empty(&ht);
    }

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

void test_hashtable_should_KeepEveryPairReachableWhileItResizes(void)
{
    enum { KEYS = 60 };

    struct int_pair pairs[KEYS];

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_init(&ht, NULL, NULL));

    for (int i = 0; i < KEYS; i++)
    {
        pairs[i].key   = i * 16; /* low bits all zero, the hard case */
        pairs[i].value = i;

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              hashtable_put(&ht, &pairs[i].key,
                                            sizeof(pairs[i].key),
                                            &pairs[i].value,
                                            sizeof(pairs[i].value)));

        /* after every single put, including the ones that resized */
        assert_contents(&ht, pairs, (size_t)i + 1);
    }

    assert_walk_yields(&ht, pairs, KEYS);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, hashtable_destroy(&ht));
}

#endif // TEST
