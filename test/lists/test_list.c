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
 * list.h publishes struct coll_list but keeps struct coll_list_node opaque,
 * so there is no public way to read an element back. These tests mirror the
 * definition from list.c in order to inspect stored elements directly.
 *
 * KEEP IN SYNC WITH list.c. If the layout there changes and this does not,
 * every check below reads the wrong bytes and can still pass.
 */
struct coll_list_node
{
    void                  *data;
    size_t                 size;
    struct coll_list_node *next;
};

static coll_list list;

void setUp(void)
{
    /* poison the handle so a field coll_list_init forgets to write shows up as
       garbage rather than as a lucky zero */
    memset(&list, 0xAA, sizeof(list));
}

void tearDown(void)
{
    /* nothing: a test that allocates nodes destroys them itself, and the
       poisoned handle above must not be walked by a blind destroy */
}

/* asserts that l is the empty list coll_list_init() and coll_list_destroy()
   promise */
static void assert_empty(const coll_list *l)
{
    TEST_ASSERT_EQUAL_size_t(0, l->size);
    TEST_ASSERT_NULL(l->first);
    TEST_ASSERT_NULL(l->last);
}

/* asserts that l holds exactly the count ints in expected, in that order, and
   that its bookkeeping agrees with the chain it actually links */
static void assert_contents(const coll_list *l, const int *expected,
                            size_t count)
{
    TEST_ASSERT_EQUAL_size_t(count, l->size);

    if (count == 0)
    {
        assert_empty(l);
        return;
    }

    TEST_ASSERT_NOT_NULL(l->first);
    TEST_ASSERT_NOT_NULL(l->last);

    size_t       seen = 0;
    struct coll_list_node *tail = NULL;

    for (struct coll_list_node *n = l->first; n != NULL; n = n->next)
    {
        TEST_ASSERT_TRUE(seen < count); /* the chain outran the expectation */
        TEST_ASSERT_NOT_NULL(n->data);
        TEST_ASSERT_EQUAL_size_t(sizeof(int), n->size);
        TEST_ASSERT_EQUAL_MEMORY(&expected[seen], n->data, sizeof(int));

        tail = n;
        seen++;
    }

    TEST_ASSERT_EQUAL_size_t(count, seen);
    TEST_ASSERT_EQUAL_PTR(l->last, tail);
    TEST_ASSERT_NULL(l->last->next);
}

/* the observable state of a list, for asserting that a call changed nothing */
struct snapshot
{
    size_t                 size;
    struct coll_list_node *first;
    struct coll_list_node *last;
};

static struct snapshot snapshot_of(const coll_list *l)
{
    struct snapshot s = { .size = l->size, .first = l->first, .last = l->last };
    return s;
}

/* asserts that l still holds exactly what it held when before was taken, down
   to the identity of the head and tail nodes */
static void assert_unchanged(const coll_list *l, struct snapshot before,
                             const int *expected, size_t count)
{
    TEST_ASSERT_EQUAL_size_t(before.size, l->size);
    TEST_ASSERT_EQUAL_PTR(before.first, l->first);
    TEST_ASSERT_EQUAL_PTR(before.last, l->last);
    assert_contents(l, expected, count);
}

/* fixture: an initialized list holding count ints, built with the API */
static void fill(coll_list *l, const int *values, size_t count)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(l, 0, NULL, 0));

    for (size_t i = 0; i < count; i++)
        TEST_ASSERT_EQUAL_INT(
            COLLECTION_OK, coll_list_append(l, &values[i], sizeof(values[i])));

    assert_contents(l, values, count);
}

/* -- INIT --------------------------------------------------------------- */

void test_list_should_InitAnEmptyListWhenSizeIsZero(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    assert_empty(&list);
}

void test_list_should_PrefillEveryElementWithACopyOfTheDefaultValue(void)
{
    const int    def   = 0x5A5A5A5A;
    const size_t count = 8;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_init(&list, count, &def, sizeof(def)));

    TEST_ASSERT_EQUAL_size_t(count, list.size);
    TEST_ASSERT_NOT_NULL(list.first);
    TEST_ASSERT_NOT_NULL(list.last);
    TEST_ASSERT_NULL(list.last->next);

    size_t seen = 0;
    struct coll_list_node *last = NULL;

    for (struct coll_list_node *n = list.first; n != NULL; n = n->next)
    {
        TEST_ASSERT_NOT_NULL(n->data);
        TEST_ASSERT_EQUAL_size_t(sizeof(def), n->size);
        TEST_ASSERT_EQUAL_MEMORY(&def, n->data, sizeof(def));
        /* a copy of the default value, not an alias of it */
        TEST_ASSERT_NOT_EQUAL_PTR(&def, n->data);

        last = n;
        seen++;
    }

    /* the chain really holds count nodes and last points at its end */
    TEST_ASSERT_EQUAL_size_t(count, seen);
    TEST_ASSERT_EQUAL_PTR(list.last, last);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectANullListPointerOnInit(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_init(NULL, 0, NULL, 0));
}

void test_list_should_RejectANullDefaultValueWhenPrefilling(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_init(&list, 4, NULL, sizeof(int)));
    assert_empty(&list);

    /* elem_size is irrelevant: the null default is reported either way */
    memset(&list, 0xAA, sizeof(list));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_init(&list, 4, NULL, 0));
    assert_empty(&list);
}

void test_list_should_RejectAZeroElementSizeWhenPrefilling(void)
{
    const int def = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL, coll_list_init(&list, 4, &def, 0));

    assert_empty(&list);
}

void test_list_should_ReportEnomemWhenAPrefilledElementCannotBeAllocated(void)
{
    const int def = 1;

    /* One element no allocator can serve. Asking for a huge element *count*
       instead would append until the machine ran out of memory, which is not
       something a test suite should do to the machine running it. */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          coll_list_init(&list, 1, &def, SIZE_MAX));

    /* the failed init still left a valid, empty, reusable list */
    assert_empty(&list);
}

/* -- DESTROY ------------------------------------------------------------ */

void test_list_should_EmptyTheListOnDestroyAndLeaveItReusable(void)
{
    const int values[] = { 1, 2, 3, 4 };

    fill(&list, values, 4);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
    assert_empty(&list);

    /* usable again without a second coll_list_init */
    const int again = 42;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &again, sizeof(again)));
    assert_contents(&list, &again, 1);

    /* and destroying an already empty list is harmless */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
    assert_empty(&list);
}

void test_list_should_RejectANullListPointerOnDestroy(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_destroy(NULL));
}

/* -- APPEND ------------------------------------------------------------- */

void test_list_should_AppendTheFirstElementAsBothFirstAndLast(void)
{
    const int value = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &value, sizeof(value)));

    TEST_ASSERT_EQUAL_size_t(1, list.size);
    TEST_ASSERT_NOT_NULL(list.first);
    TEST_ASSERT_NOT_NULL(list.last);
    TEST_ASSERT_EQUAL_PTR(list.first, list.last);
    TEST_ASSERT_NULL(list.first->next);
    TEST_ASSERT_EQUAL_MEMORY(&value, list.first->data, sizeof(value));
    TEST_ASSERT_EQUAL_size_t(sizeof(value), list.first->size);
    /* a copy of the caller's object, not an alias of it */
    TEST_ASSERT_NOT_EQUAL_PTR(&value, list.first->data);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_AppendAfterExistingElementsWithoutDisturbingThem(void)
{
    const int values[]   = { 10, 20, 30 };
    const int appended   = 40;
    const int expected[] = { 10, 20, 30, 40 };

    fill(&list, values, 3);
    struct coll_list_node *first_before = list.first;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &appended, sizeof(appended)));

    assert_contents(&list, expected, 4);
    /* the existing nodes were not moved or reallocated */
    TEST_ASSERT_EQUAL_PTR(first_before, list.first);
    /* and the new element landed at the tail */
    TEST_ASSERT_EQUAL_MEMORY(&appended, list.last->data, sizeof(appended));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectNullPointersOnAppend(void)
{
    const int value = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_append(NULL, &value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_append(&list, NULL, sizeof(value)));
    /* elem_size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_append(&list, NULL, 0));

    assert_empty(&list);
}

void test_list_should_RejectAZeroElementSizeOnAppend(void)
{
    const int value = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_list_append(&list, &value, 0));

    assert_empty(&list);
}

void test_list_should_ReportEnomemWhenAnAppendedElementCannotBeAllocated(void)
{
    const int values[] = { 10, 20 };
    const int value    = 30;

    fill(&list, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          coll_list_append(&list, &value, SIZE_MAX));

    /* a failed append leaves the list exactly as it was */
    assert_contents(&list, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

/* -- ADD_AT ------------------------------------------------------------- */

void test_list_should_AddAtIndexZeroOfAnEmptyListAsTheOnlyElement(void)
{
    const int value = 7;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_add_at(&list, 0, &value, sizeof(value)));

    assert_contents(&list, &value, 1);
    TEST_ASSERT_EQUAL_PTR(list.first, list.last);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_AddAtIndexZeroOfANonEmptyListAsTheNewHead(void)
{
    const int values[]   = { 20, 30 };
    const int inserted   = 10;
    const int expected[] = { 10, 20, 30 };

    fill(&list, values, 2);
    struct coll_list_node *last_before = list.last;

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK, coll_list_add_at(&list, 0, &inserted, sizeof(inserted)));

    assert_contents(&list, expected, 3);
    /* the old elements simply follow the new head */
    TEST_ASSERT_EQUAL_PTR(last_before, list.last);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_AddInTheMiddleShiftingTheRestUp(void)
{
    const int values[]   = { 10, 20, 30, 40 };
    const int inserted   = 99;
    const int expected[] = { 10, 20, 99, 30, 40 };

    fill(&list, values, 4);
    struct coll_list_node *first_before = list.first;
    struct coll_list_node *last_before = list.last;

    /* index is neither 0, nor size - 1, nor size */
    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK, coll_list_add_at(&list, 2, &inserted, sizeof(inserted)));

    assert_contents(&list, expected, 5);
    /* everything before the index kept its node, and the tail did not move */
    TEST_ASSERT_EQUAL_PTR(first_before, list.first);
    TEST_ASSERT_EQUAL_PTR(last_before, list.last);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_AddAtIndexEqualToSizeAsTheNewTail(void)
{
    const int values[]   = { 10, 20 };
    const int inserted   = 30;
    const int expected[] = { 10, 20, 30 };

    fill(&list, values, 2);
    struct coll_list_node *first_before = list.first;

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK, coll_list_add_at(&list, 2, &inserted, sizeof(inserted)));

    assert_contents(&list, expected, 3);
    TEST_ASSERT_EQUAL_PTR(first_before, list.first);
    TEST_ASSERT_EQUAL_MEMORY(&inserted, list.last->data, sizeof(inserted));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectNullPointersOnAddAt(void)
{
    const int value = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_add_at(NULL, 0, &value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_add_at(&list, 0, NULL, sizeof(value)));
    /* index and elem_size are irrelevant: a null pointer is reported first */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_add_at(&list, 99, NULL, 0));

    assert_empty(&list);
}

void test_list_should_RejectAZeroElementSizeOnAddAt(void)
{
    const int value = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_list_add_at(&list, 0, &value, 0));
    /* elem_size is checked before the index range */
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_list_add_at(&list, 99, &value, 0));

    assert_empty(&list);
}

void test_list_should_RejectAnIndexPastTheEndOnAddAt(void)
{
    const int values[] = { 10, 20 };
    const int value    = 30;

    fill(&list, values, 2);

    /* an index equal to the size appends, so only beyond it is out of range */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ERANGE,
                          coll_list_add_at(&list, 3, &value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ERANGE,
        coll_list_add_at(&list, SIZE_MAX, &value, sizeof(value)));

    assert_contents(&list, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReportEnomemWhenAnInsertedElementCannotBeAllocated(void)
{
    const int values[] = { 10, 20, 30 };
    const int value    = 99;

    fill(&list, values, 3);

    /* an insert in the middle allocates directly */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          coll_list_add_at(&list, 1, &value, SIZE_MAX));
    assert_contents(&list, values, 3);

    /* an index equal to the size delegates to coll_list_append, which also
       fails */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          coll_list_add_at(&list, 3, &value, SIZE_MAX));
    assert_contents(&list, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

/* -- REMOVE ------------------------------------------------------------- */

void test_list_should_RemoveTheOnlyElementLeavingTheListEmpty(void)
{
    const int values[] = { 42 };

    fill(&list, values, 1);

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK, coll_list_remove(&list, &values[0], sizeof(values[0])));

    assert_empty(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RemoveTheHeadAndPromoteTheNextElement(void)
{
    const int values[]   = { 10, 20, 30 };
    const int expected[] = { 20, 30 };

    fill(&list, values, 3);
    struct coll_list_node *last_before = list.last;

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK, coll_list_remove(&list, &values[0], sizeof(values[0])));

    assert_contents(&list, expected, 2);
    /* only the head changed */
    TEST_ASSERT_EQUAL_PTR(last_before, list.last);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RemoveTheTailAndPullBackTheLastPointer(void)
{
    const int values[]   = { 10, 20, 30 };
    const int expected[] = { 10, 20 };

    fill(&list, values, 3);
    struct coll_list_node *first_before = list.first;

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK, coll_list_remove(&list, &values[2], sizeof(values[2])));

    /* assert_contents checks that last really is the end of the chain */
    assert_contents(&list, expected, 2);
    TEST_ASSERT_EQUAL_PTR(first_before, list.first);
    TEST_ASSERT_EQUAL_MEMORY(&expected[1], list.last->data, sizeof(int));

    /* the pulled back tail must still accept an append */
    const int appended    = 40;
    const int after_append[] = { 10, 20, 40 };
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &appended, sizeof(appended)));
    assert_contents(&list, after_append, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RemoveFromTheMiddleStitchingTheNeighbours(void)
{
    const int values[]   = { 10, 20, 30, 40 };
    const int expected[] = { 10, 30, 40 };

    fill(&list, values, 4);
    struct coll_list_node *first_before = list.first;
    struct coll_list_node *last_before = list.last;

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK, coll_list_remove(&list, &values[1], sizeof(values[1])));

    assert_contents(&list, expected, 3);
    TEST_ASSERT_EQUAL_PTR(first_before, list.first);
    TEST_ASSERT_EQUAL_PTR(last_before, list.last);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RemoveOnlyTheFirstOfSeveralEqualElements(void)
{
    const int values[]   = { 7, 10, 7, 20, 7 };
    const int expected[] = { 10, 7, 20, 7 };
    const int needle     = 7;

    fill(&list, values, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_remove(&list, &needle, sizeof(needle)));

    /* exactly one copy went, and it was the leftmost one */
    assert_contents(&list, expected, 4);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectNullPointersOnRemove(void)
{
    const int values[] = { 10, 20 };

    fill(&list, values, 2);
    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENULL,
        coll_list_remove(NULL, &values[0], sizeof(values[0])));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_remove(&list, NULL, sizeof(values[0])));
    /* elem_size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_remove(&list, NULL, 0));

    assert_unchanged(&list, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectAZeroElementSizeOnRemove(void)
{
    const int values[] = { 10, 20 };

    fill(&list, values, 2);
    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_list_remove(&list, &values[0], 0));

    assert_unchanged(&list, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_NotMatchAnElementOfADifferentSizeOnRemove(void)
{
    /* The list holds 8 byte elements. The short needle holds the same value in
       4 bytes, so on a little endian machine its bytes are a prefix of the
       stored element: a min() style compare would match it, and would read
       past the needle. The long needle is the stored element followed by extra
       bytes, so a prefix compare would match that one too. Exact sizing
       rejects both. The widths are explicit because long and long long are
       both 8 bytes under LP64. */
    const uint64_t stored[] = { 5, 6 };
    const uint32_t shorter  = 5;

    unsigned char longer[sizeof(stored[0]) + 4] = { 0 };
    memcpy(longer, &stored[0], sizeof(stored[0]));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));
    for (size_t i = 0; i < 2; i++)
        TEST_ASSERT_EQUAL_INT(
            COLLECTION_OK,
            coll_list_append(&list, &stored[i], sizeof(stored[i])));

    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_list_remove(&list, &shorter, sizeof(shorter)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_list_remove(&list, longer, sizeof(longer)));

    /* neither near miss disturbed the list */
    TEST_ASSERT_EQUAL_size_t(before.size, list.size);
    TEST_ASSERT_EQUAL_PTR(before.first, list.first);
    TEST_ASSERT_EQUAL_PTR(before.last, list.last);

    /* but the same value at the stored width is found */
    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK, coll_list_remove(&list, &stored[0], sizeof(stored[0])));
    TEST_ASSERT_EQUAL_size_t(1, list.size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReportNotFoundWhenNoElementMatchesOnRemove(void)
{
    const int values[] = { 10, 20, 30 };
    const int missing  = 99;

    fill(&list, values, 3);
    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_list_remove(&list, &missing, sizeof(missing)));

    assert_unchanged(&list, before, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReportNotFoundWhenRemovingFromAnEmptyList(void)
{
    const int missing = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_list_remove(&list, &missing, sizeof(missing)));

    assert_empty(&list);
}

/* -- REMOVE_ALL --------------------------------------------------------- */

void test_list_should_RemoveEveryEqualElementKeepingTheOthers(void)
{
    const int values[]   = { 10, 7, 20, 7, 30 };
    const int expected[] = { 10, 20, 30 };
    const int needle     = 7;

    fill(&list, values, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_remove_all(&list, &needle, sizeof(needle)));

    /* survivors keep their relative order */
    assert_contents(&list, expected, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RemoveEveryElementWhenAllMatchLeavingTheListEmpty(void)
{
    const int values[] = { 7, 7, 7 };
    const int needle   = 7;

    fill(&list, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_remove_all(&list, &needle, sizeof(needle)));

    assert_empty(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RemoveMatchesAtHeadMiddleAndTailKeepingLastCorrect(void)
{
    const int values[]   = { 7, 10, 7, 20, 7 };
    const int expected[] = { 10, 20 };
    const int needle     = 7;

    fill(&list, values, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_remove_all(&list, &needle, sizeof(needle)));

    assert_contents(&list, expected, 2);

    /* the tail was rebuilt while walking, so prove it still accepts an append
       */
    const int appended       = 30;
    const int after_append[] = { 10, 20, 30 };
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &appended, sizeof(appended)));
    assert_contents(&list, after_append, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RemoveConsecutiveMatchesAtTheTail(void)
{
    const int values[]   = { 10, 20, 7, 7, 7 };
    const int expected[] = { 10, 20 };
    const int needle     = 7;

    fill(&list, values, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_remove_all(&list, &needle, sizeof(needle)));

    assert_contents(&list, expected, 2);
    TEST_ASSERT_EQUAL_MEMORY(&expected[1], list.last->data, sizeof(int));

    const int appended       = 30;
    const int after_append[] = { 10, 20, 30 };
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &appended, sizeof(appended)));
    assert_contents(&list, after_append, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectNullPointersOnRemoveAll(void)
{
    const int values[] = { 10, 20 };

    fill(&list, values, 2);
    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENULL,
        coll_list_remove_all(NULL, &values[0], sizeof(values[0])));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_remove_all(&list, NULL, sizeof(values[0])));
    /* elem_size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_remove_all(&list, NULL, 0));

    assert_unchanged(&list, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectAZeroElementSizeOnRemoveAll(void)
{
    const int values[] = { 10, 20 };

    fill(&list, values, 2);
    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_list_remove_all(&list, &values[0], 0));

    assert_unchanged(&list, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_NotMatchAnElementOfADifferentSizeOnRemoveAll(void)
{
    const uint64_t stored[] = { 5, 5 };
    const uint32_t shorter  = 5;

    unsigned char longer[sizeof(stored[0]) + 4] = { 0 };
    memcpy(longer, &stored[0], sizeof(stored[0]));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));
    for (size_t i = 0; i < 2; i++)
        TEST_ASSERT_EQUAL_INT(
            COLLECTION_OK,
            coll_list_append(&list, &stored[i], sizeof(stored[i])));

    /* every element would match under a prefix compare, none match exactly */
    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENOTFOUND,
        coll_list_remove_all(&list, &shorter, sizeof(shorter)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_list_remove_all(&list, longer, sizeof(longer)));
    TEST_ASSERT_EQUAL_size_t(2, list.size);

    /* at the stored width both go */
    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK,
        coll_list_remove_all(&list, &stored[0], sizeof(stored[0])));
    assert_empty(&list);
}

void test_list_should_ReportNotFoundWhenNoElementMatchesOnRemoveAll(void)
{
    const int values[] = { 10, 20, 30 };
    const int missing  = 99;

    fill(&list, values, 3);
    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENOTFOUND,
        coll_list_remove_all(&list, &missing, sizeof(missing)));

    assert_unchanged(&list, before, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReportNotFoundWhenRemovingAllFromAnEmptyList(void)
{
    const int missing = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENOTFOUND,
        coll_list_remove_all(&list, &missing, sizeof(missing)));

    assert_empty(&list);
}

/* -- REMOVE_AT ---------------------------------------------------------- */

void test_list_should_RemoveAtIndexZeroPromotingTheNextElement(void)
{
    const int values[]   = { 10, 20, 30 };
    const int expected[] = { 20, 30 };

    fill(&list, values, 3);
    struct coll_list_node *last_before = list.last;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_remove_at(&list, 0));

    assert_contents(&list, expected, 2);
    TEST_ASSERT_EQUAL_PTR(last_before, list.last);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RemoveAtTheLastIndexPullingBackTheLastPointer(void)
{
    const int values[]   = { 10, 20, 30 };
    const int expected[] = { 10, 20 };

    fill(&list, values, 3);
    struct coll_list_node *first_before = list.first;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_remove_at(&list, 2));

    assert_contents(&list, expected, 2);
    TEST_ASSERT_EQUAL_PTR(first_before, list.first);

    const int appended       = 40;
    const int after_append[] = { 10, 20, 40 };
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &appended, sizeof(appended)));
    assert_contents(&list, after_append, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RemoveAtAMiddleIndexShiftingTheRestDown(void)
{
    const int values[]   = { 10, 20, 30, 40 };
    const int expected[] = { 10, 30, 40 };

    fill(&list, values, 4);
    struct coll_list_node *first_before = list.first;
    struct coll_list_node *last_before = list.last;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_remove_at(&list, 1));

    assert_contents(&list, expected, 3);
    TEST_ASSERT_EQUAL_PTR(first_before, list.first);
    TEST_ASSERT_EQUAL_PTR(last_before, list.last);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RemoveTheOnlyElementByIndexLeavingTheListEmpty(void)
{
    const int values[] = { 42 };

    fill(&list, values, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_remove_at(&list, 0));

    assert_empty(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectANullListPointerOnRemoveAt(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_remove_at(NULL, 0));
}

void test_list_should_RejectAnIndexNotSmallerThanTheSizeOnRemoveAt(void)
{
    const int values[] = { 10, 20 };

    fill(&list, values, 2);
    struct snapshot before = snapshot_of(&list);

    /* unlike coll_list_add_at, an index equal to the size is already out of
       range */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ERANGE, coll_list_remove_at(&list, 2));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ERANGE, coll_list_remove_at(&list, 3));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ERANGE,
                          coll_list_remove_at(&list, SIZE_MAX));

    assert_unchanged(&list, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReportRangeErrorWhenRemovingByIndexFromAnEmptyList(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ERANGE, coll_list_remove_at(&list, 0));

    assert_empty(&list);
}

/* -- SORT --------------------------------------------------------------- */

/* the ascending order of two ints, written the way a qsort() callback is:
   subtracting would overflow on values far apart */
static int cmp_int_asc(const void *a, const void *b)
{
    const int x = *(const int *)a;
    const int y = *(const int *)b;

    return (x > y) - (x < y);
}

/* the same order reversed, to prove the list follows the caller's comparison
   rather than a built in one */
static int cmp_int_desc(const void *a, const void *b)
{
    return cmp_int_asc(b, a);
}

static size_t cmp_calls;

static int cmp_int_counting(const void *a, const void *b)
{
    cmp_calls++;
    return cmp_int_asc(a, b);
}

/* an element carrying a tag the comparison below deliberately ignores, so
   that equal keys can still be told apart afterwards */
struct tagged
{
    int key;
    int tag;
};

static int cmp_tagged_key(const void *a, const void *b)
{
    const int x = ((const struct tagged *)a)->key;
    const int y = ((const struct tagged *)b)->key;

    return (x > y) - (x < y);
}

void test_list_should_SortAnUnorderedListIntoAscendingOrder(void)
{
    const int values[]   = { 5, 3, 9, 1, 7, 2 };
    const int expected[] = { 1, 2, 3, 5, 7, 9 };

    fill(&list, values, 6);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_sort(&list, cmp_int_asc));

    assert_contents(&list, expected, 6);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_FollowTheOrderTheComparisonAsksFor(void)
{
    const int values[]   = { 5, 3, 9, 1, 7, 2 };
    const int expected[] = { 9, 7, 5, 3, 2, 1 };

    fill(&list, values, 6);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_sort(&list, cmp_int_desc));

    assert_contents(&list, expected, 6);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_LeaveAnAlreadySortedListInOrder(void)
{
    const int values[] = { 1, 2, 3, 4, 5 };

    fill(&list, values, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_sort(&list, cmp_int_asc));

    assert_contents(&list, values, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_SortAListThatIsInExactlyReverseOrder(void)
{
    const int values[]   = { 5, 4, 3, 2, 1 };
    const int expected[] = { 1, 2, 3, 4, 5 };

    fill(&list, values, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_sort(&list, cmp_int_asc));

    assert_contents(&list, expected, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_KeepEveryDuplicateWhenSorting(void)
{
    const int values[]   = { 4, 1, 4, 1, 4 };
    const int expected[] = { 1, 1, 4, 4, 4 };

    fill(&list, values, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_sort(&list, cmp_int_asc));

    /* nothing was dropped or duplicated on the way */
    assert_contents(&list, expected, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_SortAnOddNumberOfElementsAcrossSeveralMergePasses(void)
{
    /* 21 elements: an odd count that is not a power of two, so every pass
       ends with a run that has no partner to merge with */
    const int values[] = { 12, 5, 19, 3, 17, 8, 1, 14, 6, 20, 2,
                           11, 9, 18, 4, 16, 7, 15, 10, 13, 21 };
    int       expected[21];

    for (size_t i = 0; i < 21; i++)
        expected[i] = (int)i + 1;

    fill(&list, values, 21);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_sort(&list, cmp_int_asc));

    assert_contents(&list, expected, 21);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_KeepEquivalentElementsInTheirOriginalOrder(void)
{
    /* same key, distinct tags: only a stable sort keeps the tags ascending
       inside each group of equal keys */
    const struct tagged values[] = {
        { .key = 2, .tag = 0 }, { .key = 1, .tag = 1 }, { .key = 2, .tag = 2 },
        { .key = 1, .tag = 3 }, { .key = 2, .tag = 4 }, { .key = 1, .tag = 5 },
    };
    const struct tagged expected[] = {
        { .key = 1, .tag = 1 }, { .key = 1, .tag = 3 }, { .key = 1, .tag = 5 },
        { .key = 2, .tag = 0 }, { .key = 2, .tag = 2 }, { .key = 2, .tag = 4 },
    };

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    for (size_t i = 0; i < 6; i++)
        TEST_ASSERT_EQUAL_INT(
            COLLECTION_OK,
            coll_list_append(&list, &values[i], sizeof(values[i])));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_sort(&list, cmp_tagged_key));

    TEST_ASSERT_EQUAL_size_t(6, list.size);

    size_t       seen = 0;
    struct coll_list_node *tail = NULL;

    for (struct coll_list_node *n = list.first; n != NULL; n = n->next)
    {
        TEST_ASSERT_TRUE(seen < 6);
        TEST_ASSERT_EQUAL_size_t(sizeof(struct tagged), n->size);
        TEST_ASSERT_EQUAL_MEMORY(&expected[seen], n->data,
                                 sizeof(struct tagged));

        tail = n;
        seen++;
    }

    TEST_ASSERT_EQUAL_size_t(6, seen);
    TEST_ASSERT_EQUAL_PTR(list.last, tail);
    TEST_ASSERT_NULL(list.last->next);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_PullTheLastPointerToTheNewTail(void)
{
    const int values[]   = { 30, 10, 20 };
    const int expected[] = { 10, 20, 30 };

    fill(&list, values, 3);

    /* the node that ends up last is the one that used to be the head */
    struct coll_list_node *head_before = list.first;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_sort(&list, cmp_int_asc));

    assert_contents(&list, expected, 3);
    TEST_ASSERT_EQUAL_PTR(head_before, list.last);
    TEST_ASSERT_NULL(list.last->next);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RelinkTheSameNodesRatherThanReallocateThem(void)
{
    const int values[]   = { 3, 1, 2 };
    const int expected[] = { 1, 2, 3 };

    fill(&list, values, 3);

    struct coll_list_node *before[3];
    size_t       i = 0;
    for (struct coll_list_node *n = list.first; n != NULL; n = n->next)
        before[i++] = n;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_sort(&list, cmp_int_asc));

    assert_contents(&list, expected, 3);

    /* the sorted chain holds exactly the nodes the list held before, so the
       elements kept their addresses and only changed position */
    TEST_ASSERT_EQUAL_PTR(before[1], list.first);
    TEST_ASSERT_EQUAL_PTR(before[2], list.first->next);
    TEST_ASSERT_EQUAL_PTR(before[0], list.first->next->next);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_SucceedWithoutComparingWhenThereIsNothingToOrder(void)
{
    const int value = 42;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    cmp_calls = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_sort(&list, cmp_int_counting));
    assert_empty(&list);
    TEST_ASSERT_EQUAL_size_t(0, cmp_calls);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &value, sizeof(value)));

    cmp_calls = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_sort(&list, cmp_int_counting));
    assert_contents(&list, &value, 1);
    TEST_ASSERT_EQUAL_size_t(0, cmp_calls);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectNullPointersOnSort(void)
{
    const int values[] = { 3, 1, 2 };

    fill(&list, values, 3);
    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_sort(NULL, cmp_int_asc));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_sort(&list, NULL));

    /* the rejected sort left the list in the order it was already in */
    assert_unchanged(&list, before, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

/* -- REVERSE ------------------------------------------------------------ */

void test_list_should_ReverseAListIntoTheOppositeOrder(void)
{
    const int values[]   = { 1, 2, 3, 4 };
    const int expected[] = { 4, 3, 2, 1 };

    fill(&list, values, 4);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_reverse(&list));

    assert_contents(&list, expected, 4);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReverseAListOfTwoElements(void)
{
    const int values[]   = { 1, 2 };
    const int expected[] = { 2, 1 };

    /* the shortest list there is anything to do to, and the one where the
       head and the tail are the only two nodes involved */
    fill(&list, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_reverse(&list));

    assert_contents(&list, expected, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReverseAnOddNumberOfElements(void)
{
    const int values[]   = { 1, 2, 3, 4, 5 };
    const int expected[] = { 5, 4, 3, 2, 1 };

    fill(&list, values, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_reverse(&list));

    /* an odd count leaves one element in the middle with the same index it
       started with, which is the case a reverse that walks in pairs gets
       wrong */
    assert_contents(&list, expected, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_SwapTheEndsWithoutReallocatingAnyElement(void)
{
    const int values[]   = { 1, 2, 3 };
    const int expected[] = { 3, 2, 1 };

    fill(&list, values, 3);

    struct coll_list_node *first_before = NULL;
    struct coll_list_node *last_before = NULL;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_get_first(&list, &first_before));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_at(&list, 2, &last_before));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_reverse(&list));

    assert_contents(&list, expected, 3);

    /* the nodes were relinked, not rebuilt: the very node that was at the
       front is now at the back, still holding the element it always held */
    TEST_ASSERT_EQUAL_PTR(last_before, list.first);
    TEST_ASSERT_EQUAL_PTR(first_before, list.last);
    TEST_ASSERT_EQUAL_MEMORY(&values[0], list.last->data, sizeof(values[0]));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReturnToTheOriginalOrderWhenReversedTwice(void)
{
    const int values[] = { 10, 20, 30, 40, 50 };

    fill(&list, values, 5);
    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_reverse(&list));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_reverse(&list));

    /* right back where it started, down to which node is the head and which
       is the tail */
    assert_unchanged(&list, before, values, 5);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_LeaveAListOfFewerThanTwoElementsAlone(void)
{
    const int value = 42;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    /* an empty list is its own reverse */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_reverse(&list));
    assert_empty(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &value, sizeof(value)));
    struct snapshot before = snapshot_of(&list);

    /* and so is a list of one, which must come out with its head and tail
       still pointing at the same single node */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_reverse(&list));
    assert_unchanged(&list, before, &value, 1);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_KeepAppendingAtTheEndAfterAReverse(void)
{
    const int values[]   = { 1, 2, 3 };
    const int late       = 4;
    const int expected[] = { 3, 2, 1, 4 };

    fill(&list, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_reverse(&list));

    /* a reverse that turned the chain around but left the tail pointer on the
       old end would append into the middle of the list, or onto a node that
       nothing links to any more */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &late, sizeof(late)));

    assert_contents(&list, expected, 4);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReverseElementsOfDifferentSizes(void)
{
    const int8_t  small = 1;
    const int32_t mid   = 2;
    const int64_t big   = 3;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &small, sizeof(small)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &mid, sizeof(mid)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_list_append(&list, &big, sizeof(big)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_reverse(&list));

    /* reversing only relinks nodes and never looks at what they hold, so a
       list of mixed sizes is no harder for it than any other */
    TEST_ASSERT_EQUAL_size_t(3, list.size);

    struct coll_list_node *n = list.first;
    TEST_ASSERT_EQUAL_size_t(sizeof(big), n->size);
    TEST_ASSERT_EQUAL_MEMORY(&big, n->data, sizeof(big));

    n = n->next;
    TEST_ASSERT_EQUAL_size_t(sizeof(mid), n->size);
    TEST_ASSERT_EQUAL_MEMORY(&mid, n->data, sizeof(mid));

    n = n->next;
    TEST_ASSERT_EQUAL_size_t(sizeof(small), n->size);
    TEST_ASSERT_EQUAL_MEMORY(&small, n->data, sizeof(small));

    TEST_ASSERT_EQUAL_PTR(list.last, n);
    TEST_ASSERT_NULL(n->next);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectANullListPointerOnReverse(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_reverse(NULL));
}

/* -- FIND --------------------------------------------------------------- */

void test_list_should_FindTheIndexOfAMatchingElement(void)
{
    const int values[] = { 10, 20, 30, 40 };
    size_t    index    = 999;

    fill(&list, values, 4);

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK,
        coll_list_find(&list, &values[0], sizeof(values[0]), &index));
    TEST_ASSERT_EQUAL_size_t(0, index);

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK,
        coll_list_find(&list, &values[2], sizeof(values[2]), &index));
    TEST_ASSERT_EQUAL_size_t(2, index);

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK,
        coll_list_find(&list, &values[3], sizeof(values[3]), &index));
    TEST_ASSERT_EQUAL_size_t(3, index);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_FindTheFirstOfSeveralEqualElements(void)
{
    const int values[] = { 10, 7, 20, 7 };
    const int needle   = 7;
    size_t    index    = 999;

    fill(&list, values, 4);

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK, coll_list_find(&list, &needle, sizeof(needle), &index));

    /* the leftmost match, not the later duplicate */
    TEST_ASSERT_EQUAL_size_t(1, index);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_LeaveTheListUnchangedWhenFinding(void)
{
    const int values[] = { 10, 20, 30 };
    size_t    index    = 999;

    fill(&list, values, 3);
    struct snapshot before = snapshot_of(&list);

    /* a read only call, so a const handle has to be enough */
    const coll_list *readonly = &list;
    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK,
        coll_list_find(readonly, &values[1], sizeof(values[1]), &index));
    TEST_ASSERT_EQUAL_size_t(1, index);

    assert_unchanged(&list, before, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectNullPointersOnFind(void)
{
    const int values[] = { 10, 20 };
    size_t    index    = 999;

    fill(&list, values, 2);

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENULL,
        coll_list_find(NULL, &values[0], sizeof(values[0]), &index));
    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENULL,
        coll_list_find(&list, NULL, sizeof(values[0]), &index));
    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENULL,
        coll_list_find(&list, &values[0], sizeof(values[0]), NULL));
    /* elem_size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_find(&list, NULL, 0, &index));

    TEST_ASSERT_EQUAL_size_t(999, index);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectAZeroElementSizeOnFind(void)
{
    const int values[] = { 10, 20 };
    size_t    index    = 999;

    fill(&list, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_list_find(&list, &values[0], 0, &index));

    TEST_ASSERT_EQUAL_size_t(999, index);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_NotMatchAnElementOfADifferentSizeOnFind(void)
{
    const uint64_t stored[] = { 5, 6 };
    const uint32_t shorter  = 5;
    size_t         index    = 999;

    unsigned char longer[sizeof(stored[0]) + 4] = { 0 };
    memcpy(longer, &stored[0], sizeof(stored[0]));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));
    for (size_t i = 0; i < 2; i++)
        TEST_ASSERT_EQUAL_INT(
            COLLECTION_OK,
            coll_list_append(&list, &stored[i], sizeof(stored[i])));

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENOTFOUND,
        coll_list_find(&list, &shorter, sizeof(shorter), &index));
    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENOTFOUND,
        coll_list_find(&list, longer, sizeof(longer), &index));
    TEST_ASSERT_EQUAL_size_t(999, index);

    /* the same value at the stored width is found */
    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK,
        coll_list_find(&list, &stored[0], sizeof(stored[0]), &index));
    TEST_ASSERT_EQUAL_size_t(0, index);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReportNotFoundAndLeaveTheIndexUntouchedWhenNothingMatches(
    void)
{
    const int values[] = { 10, 20, 30 };
    const int missing  = 99;
    size_t    index    = 999;

    fill(&list, values, 3);
    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENOTFOUND,
        coll_list_find(&list, &missing, sizeof(missing), &index));

    TEST_ASSERT_EQUAL_size_t(999, index);
    assert_unchanged(&list, before, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReportNotFoundWhenFindingInAnEmptyList(void)
{
    const int missing = 1;
    size_t    index   = 999;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_ENOTFOUND,
        coll_list_find(&list, &missing, sizeof(missing), &index));

    TEST_ASSERT_EQUAL_size_t(999, index);
    assert_empty(&list);
}

/* -- AT ----------------------------------------------------------------- */

void test_list_should_ReturnTheHeadForIndexZero(void)
{
    const int    values[] = { 10, 20, 30 };
    struct coll_list_node *node = NULL;

    fill(&list, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_at(&list, 0, &node));

    TEST_ASSERT_EQUAL_PTR(list.first, node);
    TEST_ASSERT_EQUAL_MEMORY(&values[0], node->data, sizeof(int));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReturnTheTailForTheLastIndex(void)
{
    const int    values[] = { 10, 20, 30 };
    struct coll_list_node *node = NULL;

    fill(&list, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_at(&list, 2, &node));

    TEST_ASSERT_EQUAL_PTR(list.last, node);
    TEST_ASSERT_NULL(node->next);
    TEST_ASSERT_EQUAL_MEMORY(&values[2], node->data, sizeof(int));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReturnTheNodeAtAMiddleIndexWithItsData(void)
{
    const int    values[] = { 10, 20, 30, 40 };
    struct coll_list_node *middle = NULL;
    struct coll_list_node *head = NULL;
    struct snapshot before;

    fill(&list, values, 4);
    before = snapshot_of(&list);

    /* a read only call, so a const handle has to be enough */
    const coll_list *readonly = &list;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_at(readonly, 1, &middle));

    TEST_ASSERT_NOT_NULL(middle);
    TEST_ASSERT_EQUAL_MEMORY(&values[1], middle->data, sizeof(int));
    TEST_ASSERT_EQUAL_size_t(sizeof(int), middle->size);

    /* distinct indices really are distinct nodes, and the walk changed nothing
       */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_at(&list, 0, &head));
    TEST_ASSERT_NOT_EQUAL_PTR(head, middle);
    TEST_ASSERT_EQUAL_PTR(middle, head->next);
    assert_unchanged(&list, before, values, 4);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectNullPointersOnAt(void)
{
    const int              values[]  = { 10, 20 };
    struct coll_list_node *node      = NULL;
    struct coll_list_node *untouched = NULL;

    fill(&list, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_at(NULL, 0, &node));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_at(&list, 0, NULL));
    /* the index is irrelevant: a null pointer is reported before ERANGE */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_list_at(NULL, SIZE_MAX, &node));

    TEST_ASSERT_EQUAL_PTR(untouched, node);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectAnIndexNotSmallerThanTheSizeOnAt(void)
{
    const int    values[] = { 10, 20 };
    struct coll_list_node *node = NULL;

    fill(&list, values, 2);
    struct snapshot before = snapshot_of(&list);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ERANGE, coll_list_at(&list, 2, &node));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ERANGE, coll_list_at(&list, 3, &node));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ERANGE,
                          coll_list_at(&list, SIZE_MAX, &node));

    /* the out parameter was never written */
    TEST_ASSERT_NULL(node);
    assert_unchanged(&list, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_ReportRangeErrorWhenReadingByIndexFromAnEmptyList(void)
{
    struct coll_list_node *node = NULL;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ERANGE, coll_list_at(&list, 0, &node));

    TEST_ASSERT_NULL(node);
    assert_empty(&list);
}

/* -- GET_SIZE ----------------------------------------------------------- */

void test_list_should_TrackTheSizeAsTheListGrowsAndShrinks(void)
{
    const int values[] = { 10, 20, 30 };
    size_t    size     = 999;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_get_size(&list, &size));
    TEST_ASSERT_EQUAL_size_t(0, size);

    for (size_t i = 0; i < 3; i++)
    {
        TEST_ASSERT_EQUAL_INT(
            COLLECTION_OK,
            coll_list_append(&list, &values[i], sizeof(values[i])));
        TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_get_size(&list, &size));
        TEST_ASSERT_EQUAL_size_t(i + 1, size);
    }

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_remove_at(&list, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_get_size(&list, &size));
    TEST_ASSERT_EQUAL_size_t(2, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_get_size(&list, &size));
    TEST_ASSERT_EQUAL_size_t(0, size);
}

void test_list_should_RejectNullPointersOnGetSize(void)
{
    const int values[] = { 10, 20 };
    size_t    size     = 999;

    fill(&list, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_get_size(NULL, &size));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_get_size(&list, NULL));

    /* the out parameter was never written */
    TEST_ASSERT_EQUAL_size_t(999, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

/* -- GET_FIRST ---------------------------------------------------------- */

void test_list_should_SucceedWithANullFirstNodeForAnEmptyList(void)
{
    struct coll_list_node *first =
        (struct coll_list_node *)&list; /* a non null starting value */

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_init(&list, 0, NULL, 0));

    /* an empty list is not an error: it succeeds and reports no node */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_get_first(&list, &first));
    TEST_ASSERT_NULL(first);
}

void test_list_should_TrackTheNewHeadAfterTheOldOneIsRemoved(void)
{
    const int    values[] = { 10, 20, 30 };
    struct coll_list_node *first = NULL;

    fill(&list, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_get_first(&list, &first));
    TEST_ASSERT_EQUAL_PTR(list.first, first);
    TEST_ASSERT_EQUAL_MEMORY(&values[0], first->data, sizeof(int));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_remove_at(&list, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_get_first(&list, &first));
    TEST_ASSERT_EQUAL_PTR(list.first, first);
    TEST_ASSERT_EQUAL_MEMORY(&values[1], first->data, sizeof(int));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

void test_list_should_RejectNullPointersOnGetFirst(void)
{
    const int    values[] = { 10, 20 };
    struct coll_list_node *first = NULL;

    fill(&list, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_get_first(NULL, &first));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_list_get_first(&list, NULL));

    /* the out parameter was never written */
    TEST_ASSERT_NULL(first);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_list_destroy(&list));
}

#endif // TEST
