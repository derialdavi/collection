/*
 * Unit tests for list.
 *
 * Name cases test_list_should_<Behaviour>, one behaviour per case.
 */

#ifdef TEST

#include "unity.h"

#include "list.h"

#include <stdint.h>
#include <string.h>

/*
 * list.h publishes struct list but keeps struct node opaque, so there is no
 * public way to read an element back. These tests mirror the definition from
 * list.c in order to inspect stored elements directly.
 *
 * KEEP IN SYNC WITH list.c. If the layout there changes and this does not,
 * every check below reads the wrong bytes and can still pass.
 */
struct node
{
    void        *data;
    size_t       size;
    struct node *next;
};

static list_t list;

void setUp(void)
{
    /* poison the handle so a field list_init forgets to write shows up as
       garbage rather than as a lucky zero */
    memset(&list, 0xAA, sizeof(list));
}

void tearDown(void)
{
    /* nothing: a test that allocates nodes destroys them itself, and the
       poisoned handle above must not be walked by a blind destroy */
}

/* asserts that l is the empty list list_init() promises to leave behind */
static void assert_empty(const list_t *l)
{
    TEST_ASSERT_EQUAL_size_t(0, l->size);
    TEST_ASSERT_NULL(l->first);
    TEST_ASSERT_NULL(l->last);
}

void test_list_should_InitAnEmptyListWhenSizeIsZero(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, list_init(&list, 0, NULL, 0));

    assert_empty(&list);
}

void test_list_should_PrefillEveryElementWithACopyOfTheDefaultValue(void)
{
    const int    def   = 0x5A5A5A5A;
    const size_t count = 8;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          list_init(&list, count, &def, sizeof(def)));

    TEST_ASSERT_EQUAL_size_t(count, list.size);
    TEST_ASSERT_NOT_NULL(list.first);
    TEST_ASSERT_NOT_NULL(list.last);
    TEST_ASSERT_NULL(list.last->next);

    size_t seen = 0;
    struct node *last = NULL;

    for (struct node *n = list.first; n != NULL; n = n->next)
    {
        TEST_ASSERT_NOT_NULL(n->data);
        TEST_ASSERT_EQUAL_size_t(sizeof def, n->size);
        TEST_ASSERT_EQUAL_MEMORY(&def, n->data, sizeof def);
        /* a copy of the default value, not an alias of it */
        TEST_ASSERT_NOT_EQUAL_PTR(&def, n->data);

        last = n;
        seen++;
    }

    /* the chain really holds count nodes and last points at its end */
    TEST_ASSERT_EQUAL_size_t(count, seen);
    TEST_ASSERT_EQUAL_PTR(list.last, last);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, list_destroy(&list));
}

void test_list_should_RejectANullListPointer(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, list_init(NULL, 0, NULL, 0));
}

void test_list_should_RejectANullDefaultValueWhenPrefilling(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          list_init(&list, 4, NULL, sizeof(int)));
    assert_empty(&list);

    /* elem_size is irrelevant: the null default is reported either way */
    memset(&list, 0xAA, sizeof list);
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, list_init(&list, 4, NULL, 0));
    assert_empty(&list);
}

void test_list_should_RejectAZeroElementSizeWhenPrefilling(void)
{
    const int def = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL, list_init(&list, 4, &def, 0));

    assert_empty(&list);
}

void test_list_should_ReportEnomemWhenAnElementCannotBeAllocated(void)
{
    const int def = 1;

    /* One element no allocator can serve. Asking for a huge element *count*
       instead would append until the machine ran out of memory, which is not
       something a test suite should do to the machine running it. */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          list_init(&list, 1, &def, SIZE_MAX));

    /* the failed init still left a valid, empty, reusable list */
    assert_empty(&list);
}

#endif // TEST
