/*
 * Unit tests for queue.
 *
 * Name cases test_queue_should_<Behaviour>, one behaviour per case.
 */

#ifdef TEST

#include "unity.h"

#include "queue.h"

#include <stdint.h>
#include <string.h>

/*
 * queue.h publishes struct coll_queue but keeps struct coll_queue_node
 * opaque, so there is no public way to inspect the chain. These tests mirror
 * the definition from queue.c in order to check the links directly.
 *
 * KEEP IN SYNC WITH queue.c. If the layout there changes and this does not,
 * every check below reads the wrong bytes and can still pass.
 */
struct coll_queue_node
{
    void                   *data;
    size_t                  size;
    struct coll_queue_node *next;
};

static coll_queue queue;

void setUp(void)
{
    /* poison the handle so a field coll_queue_init forgets to write shows up as
       garbage rather than as a lucky zero */
    memset(&queue, 0xAA, sizeof(queue));
}

void tearDown(void)
{
    /* nothing: a test that allocates nodes destroys them itself, and the
       poisoned handle above must not be walked by a blind destroy */
}

/* asserts that q is the empty queue coll_queue_init() and coll_queue_destroy()
   promise */
static void assert_empty(const coll_queue *q)
{
    TEST_ASSERT_EQUAL_size_t(0, q->size);
    TEST_ASSERT_NULL(q->front);
    TEST_ASSERT_NULL(q->back);
}

/* asserts that q holds exactly the count ints in expected, front to back, and
   that its bookkeeping agrees with the chain it actually links */
static void assert_contents(const coll_queue *q, const int *expected,
                            size_t count)
{
    TEST_ASSERT_EQUAL_size_t(count, q->size);

    if (count == 0)
    {
        assert_empty(q);
        return;
    }

    TEST_ASSERT_NOT_NULL(q->front);
    TEST_ASSERT_NOT_NULL(q->back);

    size_t             seen = 0;
    struct coll_queue_node *back = NULL;

    for (struct coll_queue_node *n = q->front; n != NULL; n = n->next)
    {
        TEST_ASSERT_TRUE(seen < count); /* the chain outran the expectation */
        TEST_ASSERT_NOT_NULL(n->data);
        TEST_ASSERT_EQUAL_size_t(sizeof(int), n->size);
        TEST_ASSERT_EQUAL_MEMORY(&expected[seen], n->data, sizeof(int));

        back = n;
        seen++;
    }

    TEST_ASSERT_EQUAL_size_t(count, seen);
    TEST_ASSERT_EQUAL_PTR(q->back, back);
    TEST_ASSERT_NULL(q->back->next);
}

/* the observable state of a queue, for asserting that a call changed nothing */
struct snapshot
{
    size_t                  size;
    struct coll_queue_node *front;
    struct coll_queue_node *back;
};

static struct snapshot snapshot_of(const coll_queue *q)
{
    struct snapshot s = { .size = q->size, .front = q->front, .back = q->back };
    return s;
}

/* asserts that q still holds exactly what it held when before was taken, down
   to the identity of the front and back nodes */
static void assert_unchanged(const coll_queue *q, struct snapshot before,
                             const int *expected, size_t count)
{
    TEST_ASSERT_EQUAL_size_t(before.size, q->size);
    TEST_ASSERT_EQUAL_PTR(before.front, q->front);
    TEST_ASSERT_EQUAL_PTR(before.back, q->back);
    assert_contents(q, expected, count);
}

/* fixture: an initialized queue holding count ints, built with the API */
static void fill(coll_queue *q, const int *values, size_t count)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(q, 0, NULL, 0));

    for (size_t i = 0; i < count; i++)
        TEST_ASSERT_EQUAL_INT(
            COLLECTION_OK,
            coll_queue_enqueue(q, &values[i], sizeof(values[i])));

    assert_contents(q, values, count);
}

/* -- INIT --------------------------------------------------------------- */

void test_queue_should_InitAnEmptyQueueWhenSizeIsZero(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));

    assert_empty(&queue);
}

void test_queue_should_PrefillEveryElementWithACopyOfTheDefaultValue(void)
{
    const int    def   = 0x5A5A5A5A;
    const size_t count = 8;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_init(&queue, count, &def, sizeof(def)));

    TEST_ASSERT_EQUAL_size_t(count, queue.size);
    TEST_ASSERT_NOT_NULL(queue.front);
    TEST_ASSERT_NOT_NULL(queue.back);
    TEST_ASSERT_NULL(queue.back->next);

    size_t             seen = 0;
    struct coll_queue_node *back = NULL;

    for (struct coll_queue_node *n = queue.front; n != NULL; n = n->next)
    {
        TEST_ASSERT_NOT_NULL(n->data);
        TEST_ASSERT_EQUAL_size_t(sizeof(def), n->size);
        TEST_ASSERT_EQUAL_MEMORY(&def, n->data, sizeof(def));
        /* a copy of the default value, not an alias of it */
        TEST_ASSERT_NOT_EQUAL_PTR(&def, n->data);

        back = n;
        seen++;
    }

    /* the chain really holds count nodes and back points at its end */
    TEST_ASSERT_EQUAL_size_t(count, seen);
    TEST_ASSERT_EQUAL_PTR(queue.back, back);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_RejectANullQueuePointerOnInit(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_queue_init(NULL, 0, NULL, 0));
}

void test_queue_should_RejectANullDefaultValueWhenPrefilling(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_queue_init(&queue, 4, NULL, sizeof(int)));
    assert_empty(&queue);

    /* elem_size is irrelevant: the null default is reported either way */
    memset(&queue, 0xAA, sizeof(queue));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_queue_init(&queue, 4, NULL, 0));
    assert_empty(&queue);
}

void test_queue_should_RejectAZeroElementSizeWhenPrefilling(void)
{
    const int def = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_queue_init(&queue, 4, &def, 0));

    assert_empty(&queue);
}

void test_queue_should_ReportEnomemWhenAPrefilledElementCannotBeAllocated(void)
{
    const int def = 1;

    /* One element no allocator can serve. Asking for a huge element *count*
       instead would enqueue until the machine ran out of memory, which is not
       something a test suite should do to the machine running it. */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          coll_queue_init(&queue, 1, &def, SIZE_MAX));

    /* the failed init still left a valid, empty, reusable queue */
    assert_empty(&queue);
}

/* -- DESTROY ------------------------------------------------------------ */

void test_queue_should_EmptyTheQueueOnDestroyAndLeaveItReusable(void)
{
    const int values[] = { 1, 2, 3, 4 };

    fill(&queue, values, 4);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
    assert_empty(&queue);

    /* usable again without a second coll_queue_init */
    const int again = 42;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_enqueue(&queue, &again, sizeof(again)));
    assert_contents(&queue, &again, 1);

    /* and destroying an already empty queue is harmless */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
    assert_empty(&queue);
}

void test_queue_should_RejectANullQueuePointerOnDestroy(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_queue_destroy(NULL));
}

/* -- ENQUEUE ------------------------------------------------------------ */

void test_queue_should_EnqueueTheFirstElementAsBothFrontAndBack(void)
{
    const int value = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_enqueue(&queue, &value, sizeof(value)));

    TEST_ASSERT_EQUAL_size_t(1, queue.size);
    TEST_ASSERT_NOT_NULL(queue.front);
    TEST_ASSERT_NOT_NULL(queue.back);
    TEST_ASSERT_EQUAL_PTR(queue.front, queue.back);
    TEST_ASSERT_NULL(queue.front->next);
    TEST_ASSERT_EQUAL_MEMORY(&value, queue.front->data, sizeof(value));
    TEST_ASSERT_EQUAL_size_t(sizeof(value), queue.front->size);
    /* a copy of the caller's object, not an alias of it */
    TEST_ASSERT_NOT_EQUAL_PTR(&value, queue.front->data);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_EnqueueAtTheBackWithoutDisturbingTheElementsWaiting(void)
{
    const int values[]   = { 10, 20, 30 };
    const int enqueued   = 40;
    const int expected[] = { 10, 20, 30, 40 };

    fill(&queue, values, 3);
    struct coll_queue_node *front_before = queue.front;

    TEST_ASSERT_EQUAL_INT(
        COLLECTION_OK, coll_queue_enqueue(&queue, &enqueued, sizeof(enqueued)));

    assert_contents(&queue, expected, 4);
    /* the elements already waiting were not moved or reallocated */
    TEST_ASSERT_EQUAL_PTR(front_before, queue.front);
    /* and the new element landed at the back, behind them */
    TEST_ASSERT_EQUAL_MEMORY(&enqueued, queue.back->data, sizeof(enqueued));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_RejectNullPointersOnEnqueue(void)
{
    const int value = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_queue_enqueue(NULL, &value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_queue_enqueue(&queue, NULL, sizeof(value)));
    /* elem_size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_queue_enqueue(&queue, NULL, 0));

    assert_empty(&queue);
}

void test_queue_should_RejectAZeroElementSizeOnEnqueue(void)
{
    const int value = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_queue_enqueue(&queue, &value, 0));

    assert_empty(&queue);
}

void test_queue_should_ReportEnomemWhenAnEnqueuedElementCannotBeAllocated(void)
{
    const int values[] = { 10, 20 };
    const int value    = 30;

    fill(&queue, values, 2);
    struct snapshot before = snapshot_of(&queue);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          coll_queue_enqueue(&queue, &value, SIZE_MAX));

    /* a failed enqueue leaves the queue exactly as it was */
    assert_unchanged(&queue, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

/* -- DEQUEUE ------------------------------------------------------------ */

void test_queue_should_DequeueElementsInTheOrderTheyWereEnqueued(void)
{
    const int values[] = { 10, 20, 30, 40 };

    fill(&queue, values, 4);

    for (size_t i = 0; i < 4; i++)
    {
        int out = 0;

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              coll_queue_dequeue(&queue, &out, sizeof(out)));
        /* first in, first out */
        TEST_ASSERT_EQUAL_INT(values[i], out);

        assert_contents(&queue, &values[i + 1], 3 - i);
    }

    assert_empty(&queue);
}

void test_queue_should_ClearTheBackWhenTheLastElementIsDequeued(void)
{
    const int value = 7;
    int       out   = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_enqueue(&queue, &value, sizeof(value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(value, out);

    /* a stale back would be dereferenced by the next enqueue */
    assert_empty(&queue);

    const int again = 9;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_enqueue(&queue, &again, sizeof(again)));
    assert_contents(&queue, &again, 1);
    TEST_ASSERT_EQUAL_PTR(queue.front, queue.back);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_KeepServingElementsEnqueuedWhileItDrains(void)
{
    const int first[] = { 1, 2 };
    int       out     = 0;

    fill(&queue, first, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(1, out);

    /* enqueueing behind a partially drained queue keeps the order */
    const int late = 3;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_enqueue(&queue, &late, sizeof(late)));

    const int expected[] = { 2, 3 };
    assert_contents(&queue, expected, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(2, out);
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(3, out);

    assert_empty(&queue);
}

void test_queue_should_CopyOutExactlyTheStoredElementOnDequeue(void)
{
    const int value = 0x0BADF00D;
    /* a guard right behind the destination: a dequeue that wrote more than
       the element is worth would trample it */
    int out[2] = { 0, 0x5A5A5A5A };

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_enqueue(&queue, &value, sizeof(value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out[0], sizeof(out[0])));

    TEST_ASSERT_EQUAL_HEX32(value, out[0]);
    TEST_ASSERT_EQUAL_HEX32(0x5A5A5A5A, out[1]);

    assert_empty(&queue);
}

void test_queue_should_ReportNotfoundWhenDequeuingAnEmptyQueue(void)
{
    int out = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_queue_dequeue(&queue, &out, sizeof(out)));

    /* the destination was never written */
    TEST_ASSERT_EQUAL_INT(0x1234, out);
    assert_empty(&queue);
}

void test_queue_should_RejectNullPointersOnDequeue(void)
{
    const int values[] = { 10, 20 };
    int       out      = 0;

    fill(&queue, values, 2);
    struct snapshot before = snapshot_of(&queue);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_queue_dequeue(NULL, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_queue_dequeue(&queue, NULL, sizeof(out)));
    /* elem_size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_queue_dequeue(&queue, NULL, 0));

    assert_unchanged(&queue, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_RejectAZeroElementSizeOnDequeue(void)
{
    const int values[] = { 10, 20 };
    int       out      = 0;

    fill(&queue, values, 2);
    struct snapshot before = snapshot_of(&queue);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_queue_dequeue(&queue, &out, 0));

    assert_unchanged(&queue, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_RejectAnElementSizeThatDoesNotMatchOnDequeue(void)
{
    const int values[] = { 10, 20 };
    /* wide enough that a mismatched copy would still fit, so only the size
       check can stop it */
    long long out = 0;

    fill(&queue, values, 2);
    struct snapshot before = snapshot_of(&queue);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_queue_dequeue(&queue, &out, sizeof(out)));

    /* the destination was never written and nothing was removed, so the
       caller can ask for the right size and try again */
    TEST_ASSERT_EQUAL_INT64(0, out);
    assert_unchanged(&queue, before, values, 2);

    int right = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &right, sizeof(right)));
    TEST_ASSERT_EQUAL_INT(values[0], right);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

/* -- PEEK --------------------------------------------------------------- */

void test_queue_should_PeekTheFrontElementWithoutRemovingIt(void)
{
    const int values[] = { 10, 20, 30 };
    int       out      = 0;

    fill(&queue, values, 3);
    struct snapshot before = snapshot_of(&queue);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_peek(&queue, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(values[0], out);

    /* peeking twice hands back the same element */
    out = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_peek(&queue, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(values[0], out);

    assert_unchanged(&queue, before, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_PeekTheNewFrontAfterADequeue(void)
{
    const int values[] = { 10, 20 };
    int       out      = 0;

    fill(&queue, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out, sizeof(out)));

    out = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_peek(&queue, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(values[1], out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_ReportNotfoundWhenPeekingAnEmptyQueue(void)
{
    int out = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_queue_peek(&queue, &out, sizeof(out)));

    /* the destination was never written */
    TEST_ASSERT_EQUAL_INT(0x1234, out);
    assert_empty(&queue);
}

void test_queue_should_RejectNullPointersOnPeek(void)
{
    const int values[] = { 10, 20 };
    int       out      = 0;

    fill(&queue, values, 2);
    struct snapshot before = snapshot_of(&queue);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_queue_peek(NULL, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_queue_peek(&queue, NULL, sizeof(out)));
    /* elem_size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_queue_peek(&queue, NULL, 0));

    assert_unchanged(&queue, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_RejectAZeroElementSizeOnPeek(void)
{
    const int values[] = { 10, 20 };
    int       out      = 0;

    fill(&queue, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL, coll_queue_peek(&queue, &out, 0));

    TEST_ASSERT_EQUAL_INT(0, out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_RejectAnElementSizeThatDoesNotMatchOnPeek(void)
{
    const int values[] = { 10, 20 };
    long long out      = 0;

    fill(&queue, values, 2);
    struct snapshot before = snapshot_of(&queue);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_queue_peek(&queue, &out, sizeof(out)));

    TEST_ASSERT_EQUAL_INT64(0, out);
    assert_unchanged(&queue, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

/* -- PEEK_SIZE ---------------------------------------------------------- */

void test_queue_should_ReportTheSizeOfTheFrontElement(void)
{
    const int values[] = { 10, 20 };
    size_t    size     = 0;

    fill(&queue, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_peek_size(&queue, &size));
    TEST_ASSERT_EQUAL_size_t(sizeof(int), size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_TrackTheSizeOfEachFrontElementInAMixedQueue(void)
{
    const int    first  = 42;
    const double second = 2.5;
    const char   third  = 'z';

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_enqueue(&queue, &first, sizeof(first)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_enqueue(&queue, &second, sizeof(second)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_enqueue(&queue, &third, sizeof(third)));

    /* a caller that does not know the layout drains the queue by asking what
       the front element needs before taking it */
    size_t size = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_peek_size(&queue, &size));
    TEST_ASSERT_EQUAL_size_t(sizeof(first), size);
    int out_first = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out_first, size));
    TEST_ASSERT_EQUAL_INT(first, out_first);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_peek_size(&queue, &size));
    TEST_ASSERT_EQUAL_size_t(sizeof(second), size);
    double out_second = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out_second, size));
    /* compared as bytes: the queue copies bytes, and Unity is built here
       without double precision support */
    TEST_ASSERT_EQUAL_MEMORY(&second, &out_second, sizeof(second));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_peek_size(&queue, &size));
    TEST_ASSERT_EQUAL_size_t(sizeof(third), size);
    char out_third = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out_third, size));
    TEST_ASSERT_EQUAL_CHAR(third, out_third);

    assert_empty(&queue);
}

void test_queue_should_ReportNotfoundWhenPeekingTheSizeOfAnEmptyQueue(void)
{
    size_t size = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_queue_peek_size(&queue, &size));

    /* the out parameter was never written */
    TEST_ASSERT_EQUAL_size_t(0x1234, size);
}

void test_queue_should_RejectNullPointersOnPeekSize(void)
{
    const int values[] = { 10, 20 };
    size_t    size     = 0x1234;

    fill(&queue, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_queue_peek_size(NULL, &size));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_queue_peek_size(&queue, NULL));

    /* the out parameter was never written */
    TEST_ASSERT_EQUAL_size_t(0x1234, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

/* -- GET_SIZE ----------------------------------------------------------- */

void test_queue_should_ReportTheNumberOfElementsItHolds(void)
{
    const int values[] = { 10, 20, 30 };
    size_t    size     = 0x1234;
    int       out      = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_get_size(&queue, &size));
    TEST_ASSERT_EQUAL_size_t(0, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
    fill(&queue, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_get_size(&queue, &size));
    TEST_ASSERT_EQUAL_size_t(3, size);

    /* it follows the queue down as well as up */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_get_size(&queue, &size));
    TEST_ASSERT_EQUAL_size_t(2, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_get_size(&queue, &size));
    TEST_ASSERT_EQUAL_size_t(0, size);
}

void test_queue_should_RejectNullPointersOnGetSize(void)
{
    const int values[] = { 10, 20 };
    size_t    size     = 0x1234;

    fill(&queue, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_queue_get_size(NULL, &size));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_queue_get_size(&queue, NULL));

    /* the out parameter was never written */
    TEST_ASSERT_EQUAL_size_t(0x1234, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

/* -- IS_EMPTY ----------------------------------------------------------- */

void test_queue_should_ReportEmptyOnlyWhileItHoldsNoElements(void)
{
    const int value = 7;
    bool      empty = false;
    int       out   = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_init(&queue, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_is_empty(&queue, &empty));
    TEST_ASSERT_TRUE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_enqueue(&queue, &value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_is_empty(&queue, &empty));
    TEST_ASSERT_FALSE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_queue_dequeue(&queue, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_is_empty(&queue, &empty));
    TEST_ASSERT_TRUE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

void test_queue_should_RejectNullPointersOnIsEmpty(void)
{
    const int values[] = { 10, 20 };
    bool      empty    = true;

    fill(&queue, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_queue_is_empty(NULL, &empty));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_queue_is_empty(&queue, NULL));

    /* the out parameter was never written */
    TEST_ASSERT_TRUE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_queue_destroy(&queue));
}

#endif // TEST
