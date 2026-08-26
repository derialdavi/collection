/*
 * list - example.
 *
 * Every function in list.h, called once, in the order you would use it.
 *
 *     cc -std=c2x examples/list.c -lcollection -o list
 */

#include <collection/list.h>

#include <stdio.h>

/* coll_list_sort() takes a comparison in the same spirit as the one qsort()
   wants: negative when a comes first, positive when b does, 0 when neither */
static int cmp_int_asc(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;

    return (x > y) - (x < y);
}

int main(void)
{
    coll_list l;
    int       rc;

    /* init: 0 asks for an empty list. The last two arguments prefill it, and
       are ignored here */
    rc = coll_list_init(&l, 0, NULL, 0);
    if (rc != COLLECTION_OK) { printf("init: %d\n", rc); return 1; }

    /* append: the value is copied into the list, so it does not have to
       outlive this call */
    int value = 30;
    rc = coll_list_append(&l, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("append: %d\n", rc); return 1; }

    value = 10;
    rc = coll_list_append(&l, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("append: %d\n", rc); return 1; }

    value = 20;
    rc = coll_list_append(&l, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("append: %d\n", rc); return 1; }

    /* add_at: index 0 puts it at the front. index == size appends */
    value = 40;
    rc = coll_list_add_at(&l, 0, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("add_at: %d\n", rc); return 1; }

    /* an index past the end is COLLECTION_ERANGE, not a crash */
    rc = coll_list_add_at(&l, 99, &value, sizeof(value));
    printf("add_at index 99: %s\n",
           rc == COLLECTION_ERANGE ? "COLLECTION_ERANGE" : "??");

    /* get_size */
    size_t size = 0;
    rc = coll_list_get_size(&l, &size);
    if (rc != COLLECTION_OK) { printf("get_size: %d\n", rc); return 1; }
    printf("%zu elements: 40 30 10 20\n", size);

    /* find: the position of the first element with these bytes. An element
       matches only when its stored size matches too */
    size_t index = 0;
    value = 10;
    rc = coll_list_find(&l, &value, sizeof(value), &index);
    if (rc != COLLECTION_OK) { printf("find: %d\n", rc); return 1; }
    printf("%d is at index %zu\n", value, index);

    /* a value that is not in the list is COLLECTION_ENOTFOUND */
    value = 77;
    rc = coll_list_find(&l, &value, sizeof(value), &index);
    printf("find %d: %s\n", value,
           rc == COLLECTION_ENOTFOUND ? "COLLECTION_ENOTFOUND" : "found");

    /* sort: in place, stable, and it only relinks nodes so it never fails
       for want of memory */
    rc = coll_list_sort(&l, cmp_int_asc);
    if (rc != COLLECTION_OK) { printf("sort: %d\n", rc); return 1; }

    value = 10;
    rc = coll_list_find(&l, &value, sizeof(value), &index);
    if (rc != COLLECTION_OK) { printf("find: %d\n", rc); return 1; }
    printf("sorted, %d is now at index %zu\n", value, index);

    /* reverse: turns the order around, whatever order it is in */
    rc = coll_list_reverse(&l);
    if (rc != COLLECTION_OK) { printf("reverse: %d\n", rc); return 1; }

    rc = coll_list_find(&l, &value, sizeof(value), &index);
    if (rc != COLLECTION_OK) { printf("find: %d\n", rc); return 1; }
    printf("reversed, %d is now at index %zu\n", value, index);

    /* at: the node at a position, and get_first: the node at the front. Both
       hand back a struct coll_list_node *, which the library keeps opaque, so
       it is good for telling you an element is there and not for reading it.
       Use coll_list_find() when you want to know where a value is */
    struct coll_list_node *node = NULL;
    rc = coll_list_at(&l, 1, &node);
    if (rc != COLLECTION_OK) { printf("at: %d\n", rc); return 1; }

    rc = coll_list_at(&l, 99, &node);
    printf("at index 99: %s\n",
           rc == COLLECTION_ERANGE ? "COLLECTION_ERANGE" : "??");

    struct coll_list_node *first = NULL;
    rc = coll_list_get_first(&l, &first);
    if (rc != COLLECTION_OK) { printf("get_first: %d\n", rc); return 1; }

    /* remove: the first element with these bytes */
    value = 40;
    rc = coll_list_remove(&l, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("remove: %d\n", rc); return 1; }

    /* remove_at: by position */
    rc = coll_list_remove_at(&l, 0);
    if (rc != COLLECTION_OK) { printf("remove_at: %d\n", rc); return 1; }

    /* remove_all: every element with these bytes at once. It is
       COLLECTION_OK when at least one went, COLLECTION_ENOTFOUND when none
       did */
    value = 10;
    rc = coll_list_remove_all(&l, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("remove_all: %d\n", rc); return 1; }

    rc = coll_list_get_size(&l, &size);
    if (rc != COLLECTION_OK) { printf("get_size: %d\n", rc); return 1; }
    printf("%zu elements left\n", size);

    /* destroy: frees every element. The coll_list itself is yours, and it is
       empty and reusable afterwards */
    rc = coll_list_destroy(&l);
    if (rc != COLLECTION_OK) { printf("destroy: %d\n", rc); return 1; }

    /* init again, this time prefilled with 5 copies of one value */
    value = 7;
    rc = coll_list_init(&l, 5, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("init: %d\n", rc); return 1; }

    rc = coll_list_get_size(&l, &size);
    if (rc != COLLECTION_OK) { printf("get_size: %d\n", rc); return 1; }
    printf("prefilled with %zu copies of %d\n", size, value);

    rc = coll_list_destroy(&l);
    if (rc != COLLECTION_OK) { printf("destroy: %d\n", rc); return 1; }

    return 0;
}
