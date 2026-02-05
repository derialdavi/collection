#ifdef TEST

#define _DEFAULT_SOURCE

#include "unity.h"

#include "threadpool.h"
#include "queue.h"
#include <unistd.h>

void setUp(void)
{
}

void tearDown(void)
{
}

void test_threadpool_ShouldNotCreateWithInvalidArgument(void)
{
    threadpool *tp = threadpool_create(0);
    TEST_ASSERT_NULL(tp);
}

void test_threadpool_ShouldCreateCorrectlyWithValidArgument(void)
{
    threadpool *tp = threadpool_create(4);

    TEST_ASSERT_NOT_NULL(tp);
    TEST_ASSERT_NOT_NULL(tp->tasks);
    TEST_ASSERT_NOT_NULL(tp->threads);
    TEST_ASSERT_EQUAL_size_t(4, tp->thread_number);
    TEST_ASSERT_TRUE(pthread_mutex_trylock(&tp->queue_mutex) == 0);
    pthread_mutex_unlock(&tp->queue_mutex);
    TEST_ASSERT_TRUE(pthread_mutex_trylock(&tp->destroy_mutex) == 0);
    pthread_mutex_unlock(&tp->destroy_mutex);
    TEST_ASSERT_TRUE(pthread_cond_signal(&tp->new_task_cond) == 0);
    TEST_ASSERT_FALSE(tp->interrupt);
    TEST_ASSERT_FALSE(tp->close);
    TEST_ASSERT_EQUAL_size_t(0, tp->started);
    TEST_ASSERT_EQUAL_size_t(0, tp->terminated);

    threadpool_destroy(tp, false);
}

void test_threadpool_ShouldNotAddNullTask(void)
{
    threadpool *tp = threadpool_create(2);
    TEST_ASSERT_NOT_NULL(tp);

    bool add_result = threadpool_add(tp, NULL);
    TEST_ASSERT_FALSE(add_result);

    threadpool_destroy(tp, false);
}

void test_threadpool_ShouldExecuteAddedTask(void)
{
    threadpool *tp = threadpool_create(1);
    TEST_ASSERT_NOT_NULL(tp);

    volatile int counter = 0;

    void *task(void *argp)
    {
        volatile int *ctr = (volatile int*)argp;
        for (int i = 0; i < 100000; i++)
            (*ctr)++;
        return NULL;
    }

    struct task t;
    t.function = task;
    t.argp = (void*)&counter;

    bool add_result = threadpool_add(tp, &t);
    TEST_ASSERT_TRUE(add_result);

    threadpool_destroy(tp, false);
    TEST_ASSERT_EQUAL_INT(100000, counter);
}

void test_threadpool_ShouldHandleMoreTasksCorrectly(void)
{
    threadpool *tp = threadpool_create(2);
    TEST_ASSERT_NOT_NULL(tp);

    volatile int counter = 0;
    pthread_mutex_t counter_mutex;
    pthread_mutex_init(&counter_mutex, NULL);

    void *task(void *argp)
    {
        volatile int *ctr = (volatile int*)argp;
        for (int i = 0; i < 100000; i++)
        {
            pthread_mutex_lock(&counter_mutex);
            (*ctr)++;
            pthread_mutex_unlock(&counter_mutex);

        }
        return NULL;
    }

    struct task t;
    t.function = task;
    t.argp = (void*)&counter;

    const int num_tasks = 10;
    for (int i = 0; i < num_tasks; i++)
    {
        bool add_result = threadpool_add(tp, &t);
        TEST_ASSERT_TRUE(add_result);
    }

    threadpool_destroy(tp, false);
    pthread_mutex_destroy(&counter_mutex);
    TEST_ASSERT_EQUAL_INT(100000 * num_tasks, counter);
}

void test_threadpool_ShouldInterruptTasksOnDestroy(void)
{
    threadpool *tp = threadpool_create(10);
    TEST_ASSERT_NOT_NULL(tp);

    volatile int counter = 0;
    pthread_mutex_t counter_mutex;
    pthread_mutex_init(&counter_mutex, NULL);

    void *task(void *argp)
    {
        volatile int *ctr = (volatile int*)argp;
        for (int i = 0; i < 10; i++)
        {
            pthread_mutex_lock(&counter_mutex);
            (*ctr)++;
            pthread_mutex_unlock(&counter_mutex);
            usleep(100000); // Enusre that the task takes some time to execute, allowing us to interrupt it during threadpool destruction
        }
        return NULL;
    }

    struct task t;
    t.function = task;
    t.argp = (void*)&counter;

    const int num_tasks = 10;
    for (int i = 0; i < num_tasks; i++)
    {
        bool add_result = threadpool_add(tp, &t);
        TEST_ASSERT_TRUE(add_result);
    }

    threadpool_destroy(tp, true);
    pthread_mutex_destroy(&counter_mutex);
    TEST_ASSERT_LESS_THAN_INT_MESSAGE(10 * num_tasks, counter, "Counter addition should have been interrupted during threadpool destruction");
}

#endif // TEST
