/*
 * hashtable - example.
 *
 * Every function in hashtable.h, called once, in the order you would use it.
 *
 *     cc -std=c2x examples/hashtable.c -lcollection -o hashtable
 */

#include <collection/hashtable.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    hashtable_t ht;
    int         rc;

    /* NULL, NULL asks for the default byte hash and comparison, which work
    for ints, doubles, structs and fixed size buffers */
    rc = hashtable_init(&ht, NULL, NULL);
    if (rc != COLLECTION_OK) { printf("init: %d\n", rc); return 1; }

    /* put: the key and the value are both copied into the table */
    int key   = 1;
    int value = 100;
    rc = hashtable_put(&ht, &key, sizeof(key), &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("put: %d\n", rc); return 1; }

    key = 2; value = 200;
    rc = hashtable_put(&ht, &key, sizeof(key), &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("put: %d\n", rc); return 1; }

    /* putting a key that is already there replaces its value */
    key = 2; value = 999;
    rc = hashtable_put(&ht, &key, sizeof(key), &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("put: %d\n", rc); return 1; }

    /* get: copies the value into storage you provide */
    key = 2;
    int out = 0;
    rc = hashtable_get(&ht, &key, sizeof(key), &out, sizeof(out));
    if (rc != COLLECTION_OK) { printf("get: %d\n", rc); return 1; }
    printf("key %d -> %d\n", key, out);

    /* a key that is not there is COLLECTION_ENOTFOUND, not a crash */
    key = 42;
    rc = hashtable_get(&ht, &key, sizeof(key), &out, sizeof(out));
    printf("key %d -> %s\n", key,
        rc == COLLECTION_ENOTFOUND ? "not found" : "found");

    /* get_value_size: how big the value is, when you do not already know */
    key = 1;
    size_t value_size = 0;
    rc = hashtable_get_value_size(&ht, &key, sizeof(key), &value_size);
    if (rc != COLLECTION_OK) { printf("get_value_size: %d\n", rc); return 1; }
    printf("value of key %d is %zu bytes\n", key, value_size);

    /* contains: a missing key is the answer, not an error */
    bool present = false;
    rc = hashtable_contains(&ht, &key, sizeof(key), &present);
    if (rc != COLLECTION_OK) { printf("contains: %d\n", rc); return 1; }
    printf("contains key %d? %s\n", key, present ? "yes" : "no");

    /* get_size */
    size_t size = 0;
    rc = hashtable_get_size(&ht, &size);
    if (rc != COLLECTION_OK) { printf("get_size: %d\n", rc); return 1; }
    printf("%zu pairs\n", size);

    /* is_empty */
    bool empty = true;
    rc = hashtable_is_empty(&ht, &empty);
    if (rc != COLLECTION_OK) { printf("is_empty: %d\n", rc); return 1; }
    printf("empty? %s\n", empty ? "yes" : "no");

    /* peek_pair_size: the sizes of the next pair, without taking it */
    size_t key_size = 0;
    rc = hashtable_peek_pair_size(&ht, &key_size, &value_size);
    if (rc != COLLECTION_OK) { printf("peek_pair_size: %d\n", rc); return 1; }
    printf("next pair: %zu byte key, %zu byte value\n", key_size, value_size);

    /* get_pair: one pair per call, COLLECTION_ENOTFOUND when there are no
    more. The sizes must match the ones the pair was stored with */
    printf("every pair:\n");
    while ((rc = hashtable_get_pair(&ht, &key, sizeof(key), &value,
                                sizeof(value))) == COLLECTION_OK)
    printf("  %d -> %d\n", key, value);

    if (rc != COLLECTION_ENOTFOUND) { printf("get_pair: %d\n", rc); return 1; }

    /* rewind: start the walk over. A put or a remove does this for you */
    rc = hashtable_rewind(&ht);
    if (rc != COLLECTION_OK) { printf("rewind: %d\n", rc); return 1; }

    /* remove */
    key = 1;
    rc = hashtable_remove(&ht, &key, sizeof(key));
    if (rc != COLLECTION_OK) { printf("remove: %d\n", rc); return 1; }

    /* destroy: frees every pair. The handle itself is yours */
    rc = hashtable_destroy(&ht);
    if (rc != COLLECTION_OK) { printf("destroy: %d\n", rc); return 1; }

    /* string keys need the callbacks that stop at the NUL, so "abc" is the
    same key whether you store it with strlen() or strlen() + 1 */
    rc = hashtable_init(&ht, hashtable_hash_string, hashtable_cmp_string);
    if (rc != COLLECTION_OK) { printf("init: %d\n", rc); return 1; }

    const char *name = "answer";
    value = 42;
    rc = hashtable_put(&ht, name, strlen(name) + 1, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("put: %d\n", rc); return 1; }

    rc = hashtable_get(&ht, name, strlen(name), &out, sizeof(out));
    if (rc != COLLECTION_OK) { printf("get: %d\n", rc); return 1; }
    printf("%s -> %d\n", name, out);

    rc = hashtable_destroy(&ht);
    if (rc != COLLECTION_OK) { printf("destroy: %d\n", rc); return 1; }

    return 0;
}
