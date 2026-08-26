/*
 * Unit tests for stack.
 *
 * Name cases test_stack_should_<Behaviour>, one behaviour per case.
 */

#ifdef TEST

#include "unity.h"

#include "stack.h"

#include <stdint.h>
#include <string.h>

/*
 * stack.h publishes struct coll_stack but keeps struct coll_stack_node
 * opaque, so there is no public way to inspect the chain. These tests mirror
 * the definition from stack.c in order to check the links directly.
 *
 * KEEP IN SYNC WITH stack.c. If the layout there changes and this does not,
 * every check below reads the wrong bytes and can still pass.
 */
struct coll_stack_node
{
    void                   *data;
    size_t                  size;
    struct coll_stack_node *next;
};

static coll_stack stack;

void setUp(void)
{
    /* poison the handle so a field coll_stack_init forgets to write shows up as
       garbage rather than as a lucky zero */
    memset(&stack, 0xAA, sizeof(stack));
}

void tearDown(void)
{
    /* nothing: a test that allocates nodes destroys them itself, and the
       poisoned handle above must not be walked by a blind destroy */
}

/* asserts that s is the empty stack coll_stack_init() and coll_stack_destroy()
   promise */
static void assert_empty(const coll_stack *s)
{
    TEST_ASSERT_EQUAL_size_t(0, s->size);
    TEST_ASSERT_NULL(s->top);
}

/*
 * asserts that s holds exactly the count ints in expected and that its
 * bookkeeping agrees with the chain it actually links.
 *
 * expected is in push order, so expected[count - 1] is the element on top.
 * That way a test names its values once, in the order it pushed them, and
 * dropping the last one describes the stack after a pop.
 */
static void assert_contents(const coll_stack *s, const int *expected,
                            size_t count)
{
    TEST_ASSERT_EQUAL_size_t(count, s->size);

    if (count == 0)
    {
        assert_empty(s);
        return;
    }

    TEST_ASSERT_NOT_NULL(s->top);

    size_t             seen = 0;
    struct coll_stack_node *last = NULL;

    for (struct coll_stack_node *n = s->top; n != NULL; n = n->next)
    {
        TEST_ASSERT_TRUE(seen < count); /* the chain outran the expectation */
        TEST_ASSERT_NOT_NULL(n->data);
        TEST_ASSERT_EQUAL_size_t(sizeof(int), n->size);
        /* walking down from the top runs backwards through expected */
        TEST_ASSERT_EQUAL_MEMORY(&expected[count - 1 - seen], n->data,
                                 sizeof(int));

        last = n;
        seen++;
    }

    TEST_ASSERT_EQUAL_size_t(count, seen);
    /* the bottom of the stack really is the end of the chain */
    TEST_ASSERT_NULL(last->next);
}

/* the observable state of a stack, for asserting that a call changed nothing */
struct snapshot
{
    size_t                  size;
    struct coll_stack_node *top;
};

static struct snapshot snapshot_of(const coll_stack *s)
{
    struct snapshot snap = { .size = s->size, .top = s->top };
    return snap;
}

/* asserts that s still holds exactly what it held when before was taken, down
   to the identity of the top node */
static void assert_unchanged(const coll_stack *s, struct snapshot before,
                             const int *expected, size_t count)
{
    TEST_ASSERT_EQUAL_size_t(before.size, s->size);
    TEST_ASSERT_EQUAL_PTR(before.top, s->top);
    assert_contents(s, expected, count);
}

/* fixture: an initialized stack holding count ints, built with the API and
   pushed in array order, so values[count - 1] ends up on top */
static void fill(coll_stack *s, const int *values, size_t count)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(s, 0, NULL, 0));

    for (size_t i = 0; i < count; i++)
        TEST_ASSERT_EQUAL_INT(
            COLLECTION_OK, coll_stack_push(s, &values[i], sizeof(values[i])));

    assert_contents(s, values, count);
}

/* -- INIT --------------------------------------------------------------- */

void test_stack_should_InitAnEmptyStackWhenSizeIsZero(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));

    assert_empty(&stack);
}

void test_stack_should_PrefillEveryElementWithACopyOfTheDefaultValue(void)
{
    const int    def   = 0x5A5A5A5A;
    const size_t count = 8;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_init(&stack, count, &def, sizeof(def)));

    TEST_ASSERT_EQUAL_size_t(count, stack.size);
    TEST_ASSERT_NOT_NULL(stack.top);

    size_t             seen = 0;
    struct coll_stack_node *last = NULL;

    for (struct coll_stack_node *n = stack.top; n != NULL; n = n->next)
    {
        TEST_ASSERT_NOT_NULL(n->data);
        TEST_ASSERT_EQUAL_size_t(sizeof(def), n->size);
        TEST_ASSERT_EQUAL_MEMORY(&def, n->data, sizeof(def));
        /* a copy of the default value, not an alias of it */
        TEST_ASSERT_NOT_EQUAL_PTR(&def, n->data);

        last = n;
        seen++;
    }

    /* the chain really holds count nodes and ends at the bottom */
    TEST_ASSERT_EQUAL_size_t(count, seen);
    TEST_ASSERT_NULL(last->next);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_RejectANullStackPointerOnInit(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_init(NULL, 0, NULL, 0));
}

void test_stack_should_RejectANullDefaultValueWhenPrefilling(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_stack_init(&stack, 4, NULL, sizeof(int)));
    assert_empty(&stack);

    /* elem_size is irrelevant: the null default is reported either way */
    memset(&stack, 0xAA, sizeof(stack));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_stack_init(&stack, 4, NULL, 0));
    assert_empty(&stack);
}

void test_stack_should_RejectAZeroElementSizeWhenPrefilling(void)
{
    const int def = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_stack_init(&stack, 4, &def, 0));

    assert_empty(&stack);
}

void test_stack_should_ReportEnomemWhenAPrefilledElementCannotBeAllocated(void)
{
    const int def = 1;

    /* One element no allocator can serve. Asking for a huge element *count*
       instead would push until the machine ran out of memory, which is not
       something a test suite should do to the machine running it. */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          coll_stack_init(&stack, 1, &def, SIZE_MAX));

    /* the failed init still left a valid, empty, reusable stack */
    assert_empty(&stack);
}

/* -- DESTROY ------------------------------------------------------------ */

void test_stack_should_EmptyTheStackOnDestroyAndLeaveItReusable(void)
{
    const int values[] = { 1, 2, 3, 4 };

    fill(&stack, values, 4);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
    assert_empty(&stack);

    /* usable again without a second coll_stack_init */
    const int again = 42;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &again, sizeof(again)));
    assert_contents(&stack, &again, 1);

    /* and destroying an already empty stack is harmless */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
    assert_empty(&stack);
}

void test_stack_should_RejectANullStackPointerOnDestroy(void)
{
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_destroy(NULL));
}

/* -- PUSH --------------------------------------------------------------- */

void test_stack_should_PushTheFirstElementAsTheTop(void)
{
    const int value = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &value, sizeof(value)));

    TEST_ASSERT_EQUAL_size_t(1, stack.size);
    TEST_ASSERT_NOT_NULL(stack.top);
    /* the only element is also the bottom of the stack */
    TEST_ASSERT_NULL(stack.top->next);
    TEST_ASSERT_EQUAL_MEMORY(&value, stack.top->data, sizeof(value));
    TEST_ASSERT_EQUAL_size_t(sizeof(value), stack.top->size);
    /* a copy of the caller's object, not an alias of it */
    TEST_ASSERT_NOT_EQUAL_PTR(&value, stack.top->data);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_PushOnTopWithoutDisturbingTheElementsBelow(void)
{
    const int values[]   = { 10, 20, 30 };
    const int pushed     = 40;
    const int expected[] = { 10, 20, 30, 40 };

    fill(&stack, values, 3);
    struct coll_stack_node *top_before = stack.top;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &pushed, sizeof(pushed)));

    assert_contents(&stack, expected, 4);
    /* the new element went on top of the old one, which was not moved or
       reallocated to make room */
    TEST_ASSERT_EQUAL_PTR(top_before, stack.top->next);
    TEST_ASSERT_EQUAL_MEMORY(&pushed, stack.top->data, sizeof(pushed));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_RejectNullPointersOnPush(void)
{
    const int value = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_stack_push(NULL, &value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_stack_push(&stack, NULL, sizeof(value)));
    /* elem_size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_push(&stack, NULL, 0));

    assert_empty(&stack);
}

void test_stack_should_RejectAZeroElementSizeOnPush(void)
{
    const int value = 1;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_stack_push(&stack, &value, 0));

    assert_empty(&stack);
}

void test_stack_should_ReportEnomemWhenAPushedElementCannotBeAllocated(void)
{
    const int values[] = { 10, 20 };
    const int value    = 30;

    fill(&stack, values, 2);
    struct snapshot before = snapshot_of(&stack);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOMEM,
                          coll_stack_push(&stack, &value, SIZE_MAX));

    /* a failed push leaves the stack exactly as it was */
    assert_unchanged(&stack, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

/* -- POP ---------------------------------------------------------------- */

void test_stack_should_PopElementsInTheReverseOfTheOrderTheyWerePushed(void)
{
    const int values[] = { 10, 20, 30, 40 };

    fill(&stack, values, 4);

    for (size_t i = 4; i > 0; i--)
    {
        int out = 0;

        TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                              coll_stack_pop(&stack, &out, sizeof(out)));
        /* last in, first out */
        TEST_ASSERT_EQUAL_INT(values[i - 1], out);

        /* what is left is everything pushed before it */
        assert_contents(&stack, values, i - 1);
    }

    assert_empty(&stack);
}

void test_stack_should_ClearTheTopWhenTheLastElementIsPopped(void)
{
    const int value = 7;
    int       out   = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &value, sizeof(value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(value, out);

    /* a stale top would be dereferenced by the next push */
    assert_empty(&stack);

    const int again = 9;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &again, sizeof(again)));
    assert_contents(&stack, &again, 1);
    TEST_ASSERT_NULL(stack.top->next);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_KeepServingElementsPushedWhileItDrains(void)
{
    const int first[] = { 1, 2 };
    int       out     = 0;

    fill(&stack, first, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(2, out);

    /* pushing onto a partially drained stack still puts the newcomer first */
    const int late = 3;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &late, sizeof(late)));

    const int expected[] = { 1, 3 };
    assert_contents(&stack, expected, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(3, out);
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(1, out);

    assert_empty(&stack);
}

void test_stack_should_CopyOutExactlyTheStoredElementOnPop(void)
{
    const int value = 0x0BADF00D;
    /* a guard right behind the destination: a pop that wrote more than the
       element is worth would trample it */
    int out[2] = { 0, 0x5A5A5A5A };

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &value, sizeof(value)));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out[0], sizeof(out[0])));

    TEST_ASSERT_EQUAL_HEX32(value, out[0]);
    TEST_ASSERT_EQUAL_HEX32(0x5A5A5A5A, out[1]);

    assert_empty(&stack);
}

void test_stack_should_ReportNotfoundWhenPoppingAnEmptyStack(void)
{
    int out = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_stack_pop(&stack, &out, sizeof(out)));

    /* the destination was never written */
    TEST_ASSERT_EQUAL_INT(0x1234, out);
    assert_empty(&stack);
}

void test_stack_should_RejectNullPointersOnPop(void)
{
    const int values[] = { 10, 20 };
    int       out      = 0;

    fill(&stack, values, 2);
    struct snapshot before = snapshot_of(&stack);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_stack_pop(NULL, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_stack_pop(&stack, NULL, sizeof(out)));
    /* elem_size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_pop(&stack, NULL, 0));

    assert_unchanged(&stack, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_RejectAZeroElementSizeOnPop(void)
{
    const int values[] = { 10, 20 };
    int       out      = 0;

    fill(&stack, values, 2);
    struct snapshot before = snapshot_of(&stack);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL, coll_stack_pop(&stack, &out, 0));

    assert_unchanged(&stack, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_RejectAnElementSizeThatDoesNotMatchOnPop(void)
{
    const int values[] = { 10, 20 };
    /* wide enough that a mismatched copy would still fit, so only the size
       check can stop it */
    long long out = 0;

    fill(&stack, values, 2);
    struct snapshot before = snapshot_of(&stack);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_stack_pop(&stack, &out, sizeof(out)));

    /* the destination was never written and nothing was removed, so the
       caller can ask for the right size and try again */
    TEST_ASSERT_EQUAL_INT64(0, out);
    assert_unchanged(&stack, before, values, 2);

    int right = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &right, sizeof(right)));
    TEST_ASSERT_EQUAL_INT(values[1], right);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

/* -- PEEK --------------------------------------------------------------- */

void test_stack_should_PeekTheTopElementWithoutRemovingIt(void)
{
    const int values[] = { 10, 20, 30 };
    int       out      = 0;

    fill(&stack, values, 3);
    struct snapshot before = snapshot_of(&stack);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_peek(&stack, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(values[2], out);

    /* peeking twice hands back the same element */
    out = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_peek(&stack, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(values[2], out);

    assert_unchanged(&stack, before, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_PeekTheNewTopAfterAPop(void)
{
    const int values[] = { 10, 20 };
    int       out      = 0;

    fill(&stack, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out, sizeof(out)));

    out = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_peek(&stack, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(values[0], out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_ReportNotfoundWhenPeekingAnEmptyStack(void)
{
    int out = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_stack_peek(&stack, &out, sizeof(out)));

    /* the destination was never written */
    TEST_ASSERT_EQUAL_INT(0x1234, out);
    assert_empty(&stack);
}

void test_stack_should_RejectNullPointersOnPeek(void)
{
    const int values[] = { 10, 20 };
    int       out      = 0;

    fill(&stack, values, 2);
    struct snapshot before = snapshot_of(&stack);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_stack_peek(NULL, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL,
                          coll_stack_peek(&stack, NULL, sizeof(out)));
    /* elem_size is irrelevant: a null pointer is reported before EINVAL */
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_peek(&stack, NULL, 0));

    assert_unchanged(&stack, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_RejectAZeroElementSizeOnPeek(void)
{
    const int values[] = { 10, 20 };
    int       out      = 0;

    fill(&stack, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL, coll_stack_peek(&stack, &out, 0));

    TEST_ASSERT_EQUAL_INT(0, out);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_RejectAnElementSizeThatDoesNotMatchOnPeek(void)
{
    const int values[] = { 10, 20 };
    long long out      = 0;

    fill(&stack, values, 2);
    struct snapshot before = snapshot_of(&stack);

    TEST_ASSERT_EQUAL_INT(COLLECTION_EINVAL,
                          coll_stack_peek(&stack, &out, sizeof(out)));

    TEST_ASSERT_EQUAL_INT64(0, out);
    assert_unchanged(&stack, before, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

/* -- PEEK_SIZE ---------------------------------------------------------- */

void test_stack_should_ReportTheSizeOfTheTopElement(void)
{
    const int values[] = { 10, 20 };
    size_t    size     = 0;

    fill(&stack, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_peek_size(&stack, &size));
    TEST_ASSERT_EQUAL_size_t(sizeof(int), size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_TrackTheSizeOfEachTopElementInAMixedStack(void)
{
    const int    first  = 42;
    const double second = 2.5;
    const char   third  = 'z';

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &first, sizeof(first)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &second, sizeof(second)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &third, sizeof(third)));

    /* a caller that does not know the layout drains the stack by asking what
       the top element needs before taking it */
    size_t size = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_peek_size(&stack, &size));
    TEST_ASSERT_EQUAL_size_t(sizeof(third), size);
    char out_third = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out_third, size));
    TEST_ASSERT_EQUAL_CHAR(third, out_third);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_peek_size(&stack, &size));
    TEST_ASSERT_EQUAL_size_t(sizeof(second), size);
    double out_second = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out_second, size));
    /* compared as bytes: the stack copies bytes, and Unity is built here
       without double precision support */
    TEST_ASSERT_EQUAL_MEMORY(&second, &out_second, sizeof(second));

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_peek_size(&stack, &size));
    TEST_ASSERT_EQUAL_size_t(sizeof(first), size);
    int out_first = 0;
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out_first, size));
    TEST_ASSERT_EQUAL_INT(first, out_first);

    assert_empty(&stack);
}

void test_stack_should_ReportNotfoundWhenPeekingTheSizeOfAnEmptyStack(void)
{
    size_t size = 0x1234;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENOTFOUND,
                          coll_stack_peek_size(&stack, &size));

    /* the out parameter was never written */
    TEST_ASSERT_EQUAL_size_t(0x1234, size);
}

void test_stack_should_RejectNullPointersOnPeekSize(void)
{
    const int values[] = { 10, 20 };
    size_t    size     = 0x1234;

    fill(&stack, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_peek_size(NULL, &size));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_peek_size(&stack, NULL));

    /* the out parameter was never written */
    TEST_ASSERT_EQUAL_size_t(0x1234, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

/* -- GET_SIZE ----------------------------------------------------------- */

void test_stack_should_ReportTheNumberOfElementsItHolds(void)
{
    const int values[] = { 10, 20, 30 };
    size_t    size     = 0x1234;
    int       out      = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_get_size(&stack, &size));
    TEST_ASSERT_EQUAL_size_t(0, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
    fill(&stack, values, 3);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_get_size(&stack, &size));
    TEST_ASSERT_EQUAL_size_t(3, size);

    /* it follows the stack down as well as up */
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_get_size(&stack, &size));
    TEST_ASSERT_EQUAL_size_t(2, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_get_size(&stack, &size));
    TEST_ASSERT_EQUAL_size_t(0, size);
}

void test_stack_should_RejectNullPointersOnGetSize(void)
{
    const int values[] = { 10, 20 };
    size_t    size     = 0x1234;

    fill(&stack, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_get_size(NULL, &size));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_get_size(&stack, NULL));

    /* the out parameter was never written */
    TEST_ASSERT_EQUAL_size_t(0x1234, size);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

/* -- IS_EMPTY ----------------------------------------------------------- */

void test_stack_should_ReportEmptyOnlyWhileItHoldsNoElements(void)
{
    const int value = 7;
    bool      empty = false;
    int       out   = 0;

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_init(&stack, 0, NULL, 0));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_is_empty(&stack, &empty));
    TEST_ASSERT_TRUE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_push(&stack, &value, sizeof(value)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_is_empty(&stack, &empty));
    TEST_ASSERT_FALSE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK,
                          coll_stack_pop(&stack, &out, sizeof(out)));
    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_is_empty(&stack, &empty));
    TEST_ASSERT_TRUE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

void test_stack_should_RejectNullPointersOnIsEmpty(void)
{
    const int values[] = { 10, 20 };
    bool      empty    = true;

    fill(&stack, values, 2);

    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_is_empty(NULL, &empty));
    TEST_ASSERT_EQUAL_INT(COLLECTION_ENULL, coll_stack_is_empty(&stack, NULL));

    /* the out parameter was never written */
    TEST_ASSERT_TRUE(empty);

    TEST_ASSERT_EQUAL_INT(COLLECTION_OK, coll_stack_destroy(&stack));
}

#endif // TEST
