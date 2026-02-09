#include "hashtable.h"

#include <stdlib.h>
#include <string.h>

hashtable_pair *create_hashtable_pair(const void *key, const size_t key_size,
                                      const void *value, const size_t value_size);

void free_hashtable_pair(hashtable_pair *pair);
static bool resize_hashtable(hashtable *ht, const size_t new_size);

hashtable *hashtable_create(size_t (*hash_function)(void *key),
                            size_t (*compare_key_function)(const void *key1, const void *key2))
{
    if (hash_function == NULL || compare_key_function == NULL)
        return NULL;

    hashtable *ht = malloc(sizeof(hashtable));
    if (ht == NULL) return NULL;

    ht->buckets_size = INITIAL_BUCKETS_SIZE;
    ht->pair_number = 0;
    ht->hash_function = hash_function;
    ht->compare_key_function = compare_key_function;

    ht->buckets = calloc(ht->buckets_size, sizeof(hashtable_pair *));
    if (ht->buckets == NULL)
    {
        free(ht);
        return NULL;
    }

    return ht;
}

bool hashtable_put(hashtable *ht,
                   const void *key, const size_t key_size,
                   const void *value, const size_t value_size)
{
    if (ht == NULL || key == NULL || key_size == 0 || value == NULL || value_size == 0)
        return false;

    hashtable_pair *new_pair = create_hashtable_pair(key, key_size, value, value_size);
    if (new_pair == NULL)
        return false;

    size_t index = ht->hash_function((void *)key) % ht->buckets_size;
    hashtable_pair *current = ht->buckets[index];
    while (current != NULL)
    {
        if (ht->compare_key_function(current->key, key) == 0)
        {
            /* don't need new pair, just update */
            free_hashtable_pair(new_pair);

            free(current->value);
            current->value = malloc(value_size);
            if (current->value == NULL)
                return false;
            memcpy(current->value, value, value_size);
            current->value_size = value_size;
            return true;
        }
        current = current->next;
    }
    new_pair->next = ht->buckets[index];
    ht->buckets[index] = new_pair;
    ht->pair_number++;

    if (ht->pair_number > ht->buckets_size * 0.75)
        return resize_hashtable(ht, ht->buckets_size * 2);

    return true;
}

void *hashtable_get(const hashtable *ht, const void *key)
{
    if (ht == NULL || key == NULL)
        return NULL;

    size_t index = ht->hash_function((void *)key) % ht->buckets_size;
    hashtable_pair *current = ht->buckets[index];
    while (current != NULL)
    {
        if (ht->compare_key_function(current->key, key) == 0)
        {
            void *value_copy = malloc(current->value_size);
            if (value_copy == NULL)
                return NULL;
            memcpy(value_copy, current->value, current->value_size);
            return value_copy;
        }
        current = current->next;
    }
    return NULL;
}

bool hashtable_remove(hashtable *ht, const void *key)
{
    if (ht == NULL || key == NULL)
        return false;

    size_t index = ht->hash_function((void *)key) % ht->buckets_size;
    hashtable_pair *current = ht->buckets[index];
    hashtable_pair *previous = NULL;

    while (current != NULL)
    {
        if (ht->compare_key_function(current->key, key) == 0)
        {
            if (previous == NULL)
                ht->buckets[index] = current->next;
            else
                previous->next = current->next;

            free_hashtable_pair(current);
            ht->pair_number--;
            break;
        }
        previous = current;
        current = current->next;
    }

    if (ht->pair_number < ht->buckets_size * 0.5 && ht->buckets_size > INITIAL_BUCKETS_SIZE)
        return resize_hashtable(ht, ht->buckets_size / 2);

    return true;
}

void **hashtable_keyset(const hashtable *ht)
{
    if (ht == NULL || ht->pair_number == 0)
        return NULL;

    void **keyset = malloc((ht->pair_number + 1) * sizeof(void *));
    if (keyset == NULL)
        return NULL;

    size_t index = 0;
    for (size_t i = 0; i < ht->buckets_size; i++)
    {
        hashtable_pair *current = ht->buckets[i];
        while (current != NULL)
        {
            keyset[index++] = current->key;
            current = current->next;
        }
    }
    keyset[index] = NULL;

    return keyset;
}

bool hashtable_size(const hashtable *ht, size_t *size)
{
    if (ht == NULL || size == NULL)
        return false;
    *size = ht->pair_number;
    return true;
}

void hashtable_destroy(hashtable *ht)
{
    if (ht == NULL)
        return;

    for (size_t i = 0; i < ht->buckets_size; i++)
    {
        hashtable_pair *current = ht->buckets[i];
        while (current != NULL)
        {
            hashtable_pair *to_free = current;
            current = current->next;
            free_hashtable_pair(to_free);
        }
    }
    free(ht->buckets);
    free(ht);
}

size_t hash_string(void *key)
{
    const char *str = (const char *)key;
    size_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;

    return hash;
}

size_t hash_int(void *key)
{
    return *(int *)key;
}

size_t hash_double(void *key)
{
    double d = *(double *)key;
    size_t hash = 0;
    unsigned char *p = (unsigned char *)&d;

    for (size_t i = 0; i < sizeof(double); i++)
        hash = hash * 31 + p[i];

    return hash;
}

size_t compare_string(const void *key1, const void *key2)
{
    return strcmp((const char *)key1, (const char *)key2);
}

size_t compare_int(const void *key1, const void *key2)
{
    int int1 = *(const int *)key1;
    int int2 = *(const int *)key2;

    if (int1 < int2)
        return -1;
    else if (int1 > int2)
        return 1;
    else
        return 0;
}

size_t compare_double(const void *key1, const void *key2)
{
    double double1 = *(const double *)key1;
    double double2 = *(const double *)key2;

    if (double1 < double2)
        return -1;
    else if (double1 > double2)
        return 1;
    else
        return 0;
}

hashtable_pair *create_hashtable_pair(const void *key, const size_t key_size,
                                      const void *value, const size_t value_size)
{
    hashtable_pair *pair = malloc(sizeof(hashtable_pair));
    if (pair == NULL) return NULL;

    pair->key = malloc(key_size);
    if (pair->key == NULL)
    {
        free(pair);
        return NULL;
    }
    memcpy(pair->key, key, key_size);
    pair->key_size = key_size;

    pair->value = malloc(value_size);
    if (pair->value == NULL)
    {
        free(pair->key);
        free(pair);
        return NULL;
    }
    memcpy(pair->value, value, value_size);
    pair->value_size = value_size;

    pair->next = NULL;

    return pair;
}

void free_hashtable_pair(hashtable_pair *pair)
{
    if (pair == NULL)
        return;
    free(pair->key);
    free(pair->value);
    free(pair);
}

static bool resize_hashtable(hashtable *ht, const size_t new_size)
{
    // controls on parameters are done by the caller (hashtable_put and hashtable_remove)

    hashtable_pair **new_buckets = calloc(new_size, sizeof(hashtable_pair *));
    if (new_buckets == NULL)
        return false;

    for (size_t i = 0; i < ht->buckets_size; i++)
    {
        hashtable_pair *current = ht->buckets[i];
        while (current != NULL)
        {
            hashtable_pair *next_pair = current->next;
            size_t new_index = ht->hash_function(current->key) % new_size;
            current->next = new_buckets[new_index];
            new_buckets[new_index] = current;
            current = next_pair;
        }
    }

    free(ht->buckets);
    ht->buckets = new_buckets;
    ht->buckets_size = new_size;

    return true;
}
