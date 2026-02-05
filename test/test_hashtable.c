#ifdef TEST

#include "unity.h"

#include "hashtable.h"

size_t simple_hash_function(void *key)
{
    // Simple hash function for testing purposes
    return (*(int *)key) % 10;
}

size_t simple_compare_key_function(const void *key1, const void *key2)
{
    // Simple key comparison function for testing purposes
    return (*(int *)key1) - (*(int *)key2);
}

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

#endif // TEST
