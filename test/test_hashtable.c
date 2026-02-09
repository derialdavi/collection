#ifdef TEST

#include "unity.h"

#include "hashtable.h"
#include <string.h>
#include <stdlib.h>

#define CHECK_INVALID_INSERTION(ht, key, key_size, value, value_size) \
    { \
        result = hashtable_put(ht, key, key_size, value, value_size); \
        TEST_ASSERT_FALSE(result); \
        if (ht != NULL) \
            TEST_ASSERT_EQUAL_INT(0, ((hashtable*)ht)->pair_number); \
    }
/* =================== UTILITIES =================== */
size_t simple_hash_function(void *key)
{
    // Simple hash function for testing purposes
    return (*(int *)key);
}

size_t simple_compare_key_function(const void *key1, const void *key2)
{
    // Simple key comparison function for testing purposes
    return (*(int *)key1) - (*(int *)key2);
}

void print_hashtable(const hashtable *ht)
{
    for (size_t i = 0; i < ht->buckets_size; i++)
    {
        printf("Bucket %zu: \n", i);
        hashtable_pair *current = ht->buckets[i];
        while (current != NULL)
        {
            printf("   Key = %d, Value = %d\n", *(int *)current->key, *(int *)current->value);
            current = current->next;
        }
    }
}
/* ================================================ */

void setUp(void)
{
}

void tearDown(void)
{
}

void test_hashtable_CreationWithInvalidPointersFunctionShouldReturnNull(void)
{
    hashtable *ht = hashtable_create(NULL, NULL);
    TEST_ASSERT_NULL(ht);

    ht = hashtable_create(simple_hash_function, NULL);
    TEST_ASSERT_NULL(ht);

    ht = hashtable_create(NULL, simple_compare_key_function);
    TEST_ASSERT_NULL(ht);
}

void test_hashtable_CreationWithValidPointersFunctionShouldReturnNotNull(void)
{
    hashtable *ht = hashtable_create(simple_hash_function, simple_compare_key_function);
    TEST_ASSERT_NOT_NULL(ht);

    TEST_ASSERT_NOT_NULL(ht->buckets);
    TEST_ASSERT_EQUAL(INITIAL_BUCKETS_SIZE, ht->buckets_size);
    TEST_ASSERT_EQUAL(0, ht->pair_number);
    TEST_ASSERT_EQUAL_PTR(simple_hash_function, ht->hash_function);
    TEST_ASSERT_EQUAL_PTR(simple_compare_key_function, ht->compare_key_function);

    hashtable_destroy(ht);
}

void test_hashtable_PutWithInvalidArgumentsShouldReturnFalse(void)
{
    int key = 1;
    int value = 10;
    bool result;
    hashtable *ht = hashtable_create(simple_hash_function, simple_compare_key_function);
    TEST_ASSERT_NOT_NULL(ht);

    CHECK_INVALID_INSERTION(NULL, NULL, 0, NULL, 0);
    CHECK_INVALID_INSERTION(NULL, NULL, 0, NULL, sizeof(value));
    CHECK_INVALID_INSERTION(NULL, NULL, 0, &value, 0);
    CHECK_INVALID_INSERTION(NULL, NULL, 0, &value, sizeof(value));
    CHECK_INVALID_INSERTION(NULL, NULL, sizeof(key), NULL, 0);
    CHECK_INVALID_INSERTION(NULL, NULL, sizeof(key), NULL, sizeof(value));
    CHECK_INVALID_INSERTION(NULL, NULL, sizeof(key), &value, 0);
    CHECK_INVALID_INSERTION(NULL, NULL, sizeof(key), &value, sizeof(value));
    CHECK_INVALID_INSERTION(NULL, &key, 0, NULL, 0);
    CHECK_INVALID_INSERTION(NULL, &key, 0, NULL, sizeof(value));
    CHECK_INVALID_INSERTION(NULL, &key, 0, &value, 0);
    CHECK_INVALID_INSERTION(NULL, &key, 0, &value, sizeof(value));
    CHECK_INVALID_INSERTION(NULL, &key, sizeof(key), NULL, 0);
    CHECK_INVALID_INSERTION(NULL, &key, sizeof(key), NULL, sizeof(value));
    CHECK_INVALID_INSERTION(NULL, &key, sizeof(key), &value, 0);
    CHECK_INVALID_INSERTION(NULL, &key, sizeof(key), &value, sizeof(value));
    CHECK_INVALID_INSERTION(ht, NULL, 0, NULL, 0);
    CHECK_INVALID_INSERTION(ht, NULL, 0, NULL, sizeof(value));
    CHECK_INVALID_INSERTION(ht, NULL, 0, &value, 0);
    CHECK_INVALID_INSERTION(ht, NULL, 0, &value, sizeof(value));
    CHECK_INVALID_INSERTION(ht, NULL, sizeof(key), NULL, 0);
    CHECK_INVALID_INSERTION(ht, NULL, sizeof(key), NULL, sizeof(value));
    CHECK_INVALID_INSERTION(ht, NULL, sizeof(key), &value, 0);
    CHECK_INVALID_INSERTION(ht, NULL, sizeof(key), &value, sizeof(value));
    CHECK_INVALID_INSERTION(ht, &key, 0, NULL, 0);
    CHECK_INVALID_INSERTION(ht, &key, 0, NULL, sizeof(value));
    CHECK_INVALID_INSERTION(ht, &key, 0, &value, 0);
    CHECK_INVALID_INSERTION(ht, &key, 0, &value, sizeof(value));
    CHECK_INVALID_INSERTION(ht, &key, sizeof(key), NULL, 0);
    CHECK_INVALID_INSERTION(ht, &key, sizeof(key), NULL, sizeof(value));
    CHECK_INVALID_INSERTION(ht, &key, sizeof(key), &value, 0);

    hashtable_destroy(ht);
}

void test_hashtable_PutWithValidArgumentsShouldReturnTrue(void)
{
    int key = 1;
    int value = 10;
    bool result;
    hashtable *ht = hashtable_create(simple_hash_function, simple_compare_key_function);
    TEST_ASSERT_NOT_NULL(ht);

    result = hashtable_put(ht, &key, sizeof(key), &value, sizeof(value));
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, ht->pair_number);
    TEST_ASSERT_NOT_NULL(ht->buckets[1 % ht->buckets_size]);
    hashtable_pair *pair = ht->buckets[1 % ht->buckets_size];
    TEST_ASSERT_EQUAL_INT(1, *(int *)pair->key);
    TEST_ASSERT_EQUAL_INT(10, *(int *)pair->value);
    TEST_ASSERT_NULL(pair->next);

    result = hashtable_put(ht, &key, sizeof(key), "updated value", strlen("updated value") + 1);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, ht->pair_number);
    TEST_ASSERT_EQUAL_INT(1, *(int *)pair->key);
    TEST_ASSERT_EQUAL_STRING("updated value", (char *)pair->value);
    TEST_ASSERT_NULL(pair->next);

    int key2 = 2;
    char *value2 = "another value";
    result = hashtable_put(ht, &key2, sizeof(key2), value2, strlen(value2) + 1);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(2, ht->pair_number);
    TEST_ASSERT_NOT_NULL(ht->buckets[2 % ht->buckets_size]);
    hashtable_pair *pair2 = ht->buckets[2 % ht->buckets_size];
    TEST_ASSERT_EQUAL_INT(2, *(int *)pair2->key);
    TEST_ASSERT_EQUAL_STRING("another value", (char *)pair2->value);
    TEST_ASSERT_NULL(pair2->next);

    hashtable_destroy(ht);
}

void test_hashtable_ResizeShouldWorkCorrectly(void)
{
    /* this function also test the get and remove functions actually */
    hashtable *ht = hashtable_create(simple_hash_function, simple_compare_key_function);
    TEST_ASSERT_NOT_NULL(ht);

    int keys[INITIAL_BUCKETS_SIZE - 1], values[INITIAL_BUCKETS_SIZE - 1];
    for (int i = 0; i < INITIAL_BUCKETS_SIZE - 1; i++)
    {
        keys[i] = i;
        values[i] = i * 10;
        hashtable_put(ht, &keys[i], sizeof(keys[i]), &values[i], sizeof(values[i]));
    }

    TEST_ASSERT_EQUAL(INITIAL_BUCKETS_SIZE - 1, ht->pair_number);
    TEST_ASSERT_TRUE(ht->buckets_size > INITIAL_BUCKETS_SIZE);

    for (int i = 0; i < INITIAL_BUCKETS_SIZE - 1; i++)
    {
        int *retrieved_value = hashtable_get(ht, &keys[i]);
        TEST_ASSERT_NOT_NULL(retrieved_value);
        TEST_ASSERT_EQUAL_INT(values[i], *retrieved_value);
        free(retrieved_value);
    }

    for (int i = 0; i < INITIAL_BUCKETS_SIZE - 3; i++)
    {
        bool removed = hashtable_remove(ht, &keys[i]);
        TEST_ASSERT_TRUE(removed);
    }
    TEST_ASSERT_EQUAL_INT(INITIAL_BUCKETS_SIZE, ht->buckets_size);

    for (int i = INITIAL_BUCKETS_SIZE - 3; i < INITIAL_BUCKETS_SIZE - 1; i++)
    {
        int *retrieved_value = hashtable_get(ht, &keys[i]);
        TEST_ASSERT_NOT_NULL(retrieved_value);
        TEST_ASSERT_EQUAL_INT(values[i], *retrieved_value);
        free(retrieved_value);
    }

    hashtable_destroy(ht);
}

void test_hashtable_KeysetShouldReturnAllKeys(void)
{
    hashtable *ht = hashtable_create(simple_hash_function, simple_compare_key_function);
    TEST_ASSERT_NOT_NULL(ht);

    int keys[5] = {1, 2, 3, 4, 5};
    char *values[5] = {"one", "two", "three", "four", "five"};
    for (int i = 0; i < 5; i++)
        hashtable_put(ht, &keys[i], sizeof(keys[i]), values[i], strlen(values[i]) + 1);

    void **keyset = hashtable_keyset(ht);
    TEST_ASSERT_NOT_NULL(keyset);

    for (int i = 0; i < 5; i++)
    {
        int *key_in_keyset = NULL;
        key_in_keyset = hashtable_get(ht, &keys[i]);
        TEST_ASSERT_NOT_NULL(key_in_keyset);
        free(key_in_keyset);
    }

    free(keyset);
    hashtable_destroy(ht);
}

void test_hashtable_SizeShouldReturnCorrectSize(void)
{
    hashtable *ht = hashtable_create(simple_hash_function, simple_compare_key_function);
    TEST_ASSERT_NOT_NULL(ht);

    size_t size;
    bool result = hashtable_size(ht, &size);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(0, size);

    int key = 1;
    int value = 10;
    hashtable_put(ht, &key, sizeof(key), &value, sizeof(value));

    result = hashtable_size(ht, &size);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(1, size);

    hashtable_remove(ht, &key);
    result = hashtable_size(ht, &size);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(0, size);

    hashtable_destroy(ht);
}

#endif // TEST
