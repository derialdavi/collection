#ifdef TEST

#include "unity.h"

#include "queue.h"

#include <string.h>

#define CHECK_NOTHING_ADDED \
    { \
        TEST_ASSERT_EQUAL_PTR(NULL, q->head); \
        TEST_ASSERT_EQUAL_PTR(NULL, q->tail); \
        TEST_ASSERT_EQUAL_INT(0, q->size); \
    }

#define INIT_DEFAULT_VALUES \
    int val = 10; \
    queue *q = malloc(sizeof(queue)); \
    if (q == NULL) return; \
    q->head = q->tail = NULL; \
    q->size = 0;

void setUp(void)
{
}

void tearDown(void)
{
}

void test_queue_should_ReturnNotNullOnCreate(void)
{
    queue *q = queue_create();
    TEST_ASSERT_NOT_NULL(q);
    free(q);
}

void test_queue_should_CreateCorrectly(void)
{
    queue *q = queue_create();
    TEST_ASSERT_NOT_NULL(q);

    TEST_ASSERT_EQUAL_PTR(NULL, q->head);
    TEST_ASSERT_EQUAL_PTR(NULL, q->tail);
    TEST_ASSERT_EQUAL_INT(0, q->size);
    free(q);
}

void test_queue_should_RetrunFalseWithInvalidArgumentsOnEnque(void)
{
    INIT_DEFAULT_VALUES

    TEST_ASSERT_FALSE(queue_enque(NULL, NULL, 0));
    CHECK_NOTHING_ADDED;

    TEST_ASSERT_FALSE(queue_enque(NULL, NULL, 4));
    CHECK_NOTHING_ADDED;

    TEST_ASSERT_FALSE(queue_enque(NULL, &val, 0));
    CHECK_NOTHING_ADDED;

    TEST_ASSERT_FALSE(queue_enque(NULL, &val, 4));
    CHECK_NOTHING_ADDED;

    TEST_ASSERT_FALSE(queue_enque(q, NULL, 0));
    CHECK_NOTHING_ADDED;

    TEST_ASSERT_FALSE(queue_enque(q, NULL, 4));
    CHECK_NOTHING_ADDED;

    TEST_ASSERT_FALSE(queue_enque(q, &val, 0));
    CHECK_NOTHING_ADDED;

    free(q);
}

void test_queue_should_EnqueOneCorrectly(void)
{
    INIT_DEFAULT_VALUES

    TEST_ASSERT_TRUE(queue_enque(q, &val, sizeof(int)));

    TEST_ASSERT_NOT_NULL(q->head);
    TEST_ASSERT_NOT_NULL(q->tail);

    TEST_ASSERT_EQUAL_PTR(q->head, q->tail);

    TEST_ASSERT_EQUAL_INT(10, *(int*)q->head->data);
    TEST_ASSERT_EQUAL_INT(10, *(int*)q->tail->data);

    TEST_ASSERT_EQUAL_INT(sizeof(int), q->head->size);
    TEST_ASSERT_EQUAL_INT(sizeof(int), q->tail->size);

    TEST_ASSERT_EQUAL_PTR(NULL, q->head->next);
    TEST_ASSERT_EQUAL_PTR(NULL, q->tail->next);

    TEST_ASSERT_EQUAL_INT(1, q->size);

    free(q->head->data);
    free(q->head);
    free(q);
}

void test_queue_should_EnqueMultipleCorrectly(void)
{
    INIT_DEFAULT_VALUES

    TEST_ASSERT_TRUE(queue_enque(q, &val, sizeof(int)));
    val = 20;
    TEST_ASSERT_TRUE(queue_enque(q, &val, sizeof(int)));

    TEST_ASSERT_NOT_NULL(q->head);
    TEST_ASSERT_NOT_NULL(q->tail);
    TEST_ASSERT_TRUE(q->head != q->tail);

    TEST_ASSERT_EQUAL_INT(20, *(int*)q->tail->data);
    TEST_ASSERT_EQUAL_INT(20, *(int*)q->head->next->data);
    TEST_ASSERT_EQUAL_INT(sizeof(int), q->head->size);
    TEST_ASSERT_EQUAL_INT(sizeof(int), q->tail->size);

    TEST_ASSERT_EQUAL_PTR(q->head->next, q->tail);
    TEST_ASSERT_EQUAL_PTR(NULL, q->tail->next);

    TEST_ASSERT_EQUAL_INT(2, q->size);

    free(q->tail->data);
    free(q->tail);
    free(q->head->data);
    free(q->head);
    free(q);
}

void test_queue_should_ReturnNullWithInvalidArgumentsOnDequeue(void)
{
    INIT_DEFAULT_VALUES
    (void)val;

    TEST_ASSERT_EQUAL_PTR(NULL, queue_deque(NULL));
    TEST_ASSERT_EQUAL_PTR(NULL, queue_deque(q));

    free(q);
}

void test_queue_should_ReturnCorrectDataOnDequeueWithOneNode(void)
{
    INIT_DEFAULT_VALUES

    q->head = malloc(sizeof(queue_node));
    if (q->head == NULL) return;
    q->head->data = malloc(sizeof(int));
    if(q->head->data == NULL) return;
    memcpy(q->head->data, &val, sizeof(int));
    q->head->size = sizeof(int);
    q->head->next = NULL;
    q->tail = q->head;
    q->size = 1;

    int *retval = queue_deque(q);
    TEST_ASSERT_NOT_NULL(retval);
    TEST_ASSERT_EQUAL_INT(10, *retval);
    TEST_ASSERT_EQUAL_PTR(NULL, q->head);
    TEST_ASSERT_EQUAL_PTR(NULL, q->tail);
    TEST_ASSERT_EQUAL_INT(0, q->size);

    free(retval);
    free(q);
}

void test_queue_should_ReturnCorrectDataOnDequeueWithMultipleNodes(void)
{
    INIT_DEFAULT_VALUES

    q->head = malloc(sizeof(queue_node));
    if (q->head == NULL) return;
    q->head->data = malloc(sizeof(int));
    if(q->head->data == NULL) return;
    memcpy(q->head->data, &val, sizeof(int));
    q->head->size = sizeof(int);

    val = 20;
    q->head->next = malloc(sizeof(queue_node));
    if (q->head->next == NULL) return;
    q->tail = q->head->next;

    q->tail->data = malloc(sizeof(int));
    if(q->tail->data == NULL) return;
    memcpy(q->tail->data, &val, sizeof(int));
    q->tail->size = sizeof(int);
    q->tail->next = NULL;

    q->size = 2;

    int *retval = queue_deque(q);
    TEST_ASSERT_NOT_NULL(retval);
    TEST_ASSERT_EQUAL_INT(10, *retval);
    TEST_ASSERT_NOT_NULL(q->head);
    TEST_ASSERT_NOT_NULL(q->tail);
    TEST_ASSERT_EQUAL_PTR(q->head, q->tail);
    TEST_ASSERT_EQUAL_INT(1, q->size);
    free(retval);

    retval = queue_deque(q);
    TEST_ASSERT_NOT_NULL(retval);
    TEST_ASSERT_EQUAL_INT(20, *retval);
    TEST_ASSERT_EQUAL_PTR(NULL, q->head);
    TEST_ASSERT_EQUAL_PTR(NULL, q->tail);

    TEST_ASSERT_EQUAL_PTR(NULL, q->head);
    TEST_ASSERT_EQUAL_PTR(NULL, q->tail);
    TEST_ASSERT_EQUAL_INT(0, q->size);

    free(retval);
    free(q);
}

#endif // TEST
