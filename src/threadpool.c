#include "threadpool.h"

#include <stdlib.h>

void *thread_routine(void *poolp);

threadpool *threadpool_create(const size_t thread_number)
{
    if (thread_number <= 0) return NULL;

    threadpool *tp = malloc(sizeof(threadpool));
    if (tp == NULL) return NULL;

    tp->tasks = queue_create();
    if (tp->tasks == NULL) return NULL;

    pthread_mutex_init(&tp->queue_mutex, NULL);
    pthread_mutex_init(&tp->destroy_mutex, NULL);
    pthread_cond_init(&tp->new_task_cond, NULL);

    tp->interrupt = tp->close = false;
    tp->thread_number = thread_number;
    tp->started = tp->terminated = 0;

    tp->threads = malloc(sizeof(pthread_t) * thread_number);
    if (tp->threads == NULL) return NULL;
    for (size_t i = 0; i < thread_number; i++)
        pthread_create(&tp->threads[i], NULL, thread_routine, (void*)tp);

    return tp;
}

void threadpool_destroy(threadpool *tp, const bool interrupt)
{
    if (tp == NULL) return;

    /* stop threads */
    pthread_mutex_lock(&tp->destroy_mutex);
    tp->interrupt = interrupt;
    tp->close = true;
    pthread_mutex_unlock(&tp->destroy_mutex);
    pthread_cond_broadcast(&tp->new_task_cond);

    for (size_t i = 0; i < tp->thread_number; i++)
        pthread_join(tp->threads[i], NULL);

    /* free memory */
    queue_destroy(tp->tasks);
    free(tp->threads);
    pthread_mutex_destroy(&tp->queue_mutex);
    pthread_mutex_destroy(&tp->destroy_mutex);
    pthread_cond_destroy(&tp->new_task_cond);
    free(tp);
}

bool threadpool_add(threadpool *tp, const struct task *task)
{
    if (tp == NULL || task == NULL)
        return false;

    /* add task to queue */
    pthread_mutex_lock(&tp->queue_mutex);
    if (queue_enque(tp->tasks, task, sizeof(struct task)) == false)
        return false;
    pthread_mutex_unlock(&tp->queue_mutex);

    /* notify waiting threads */
    pthread_cond_broadcast(&tp->new_task_cond);

    return true;
}

void *thread_routine(void *poolp)
{
    threadpool *pool = poolp;

    while (true) // keeps the thread alive
    {
        pthread_mutex_lock(&pool->queue_mutex);
        while (pool->tasks->size == 0 && !pool->close)
            pthread_cond_wait(&pool->new_task_cond, &pool->queue_mutex);

        /* force interrupt */
        pthread_mutex_lock(&pool->destroy_mutex);
        if (pool->interrupt)
        {
            pthread_mutex_unlock(&pool->destroy_mutex);
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }

        /* finish all tasks and then close*/
        if (pool->close && pool->tasks->size == 0 && pool->started == pool->terminated)
        {
            pthread_mutex_unlock(&pool->destroy_mutex);
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }
        pthread_mutex_unlock(&pool->destroy_mutex);

        /**
         * if we get past the loop it means that there is a taks to
         * do and we have the lock.
         */
        struct task *task = queue_deque(pool->tasks);
        pthread_mutex_unlock(&pool->queue_mutex);

        if (task != NULL)
        {
            pthread_mutex_lock(&pool->queue_mutex);
            pool->started++;
            pthread_mutex_unlock(&pool->queue_mutex);
            task->function(task->argp);
            pthread_mutex_lock(&pool->queue_mutex);
            pool->terminated++;
            pthread_mutex_unlock(&pool->queue_mutex);
            free(task);
        }
    }

    pthread_exit(NULL);
    return NULL;
}
