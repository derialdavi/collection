#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#include "queue.h"
#include <pthread.h>

struct task
{
    void *(*function)(void*);
    void *argp;
};

typedef struct
{
    queue           *tasks;
    pthread_t       *threads;
    size_t           thread_number;
    pthread_mutex_t  queue_mutex;
    pthread_mutex_t  destroy_mutex;
    pthread_cond_t   new_task_cond;
    bool             interrupt;
    bool             close;
    size_t           started;
    size_t           terminated;
} threadpool;

/**
 * @brief creates a new threadpool object containing a specified number
 * of threads
 *
 * @param thread_number number of available cuncurrent threads
 * @return threadpool* pointer to the newly created threadpool
 */
threadpool *threadpool_create(const size_t thread_number);

/**
 * @brief closes the threads and frees the memory taken by the threadpool
 *
 * @param tp pointer to the threadpool to destroy
 * @param interrupt indicates to destroy the threadpool after finishing
 * already present tasks (false), or destroy the threadpool immidiately,
 * ignoring next tasks (true)
 */
void threadpool_destroy(threadpool *tp, const bool interrupt);

/**
 * @brief add a task to be done by one of the threads
 *
 * @param tp pointer to the threadpool to add the task
 * @param task pointer to the task to add
 * @return true on success add
 * @return false on failure
 */
bool threadpool_add(threadpool *tp, const struct task *task);

#endif
