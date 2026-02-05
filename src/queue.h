#ifndef __QUEUE_H__
#define __QUEUE_H__

#include <stdlib.h>

typedef struct node
{
    void        *data;
    size_t       size;
    struct node *next;
} queue_node;

typedef struct
{
    queue_node *head;
    queue_node *tail;
    size_t      size;
} queue;

/**
 * @brief creates a new queue object
 *
 * @return queue* pointer to the newly allocated queue
 */
queue *queue_create();

/**
 * @brief insert a new generic element in the queue
 *
 * @param q the pointer to the queue to insert the element
 * @param data pointer to the data to be stored
 * @param size size of the data to be stored
 * @return true on success
 * @return false on failure
 */
bool queue_enque(queue *q, const void *data, const size_t size);

/**
 * @brief removes the first element of the queue
 *
 * @param q pointer to the queue you want to get the data from
 * @return void* dynamically allocated memory containing the copy of
 * the data. The programmer is responsible to free the memory returned
 * by this function
 */
void *queue_deque(queue *q);

/**
 * @brief returns the number of elements in the queue
 *
 * @param q pointer to the queue you want to get the size
 * @return int -1 if the pointer is null, the size of the queue otherwise
 */
int queue_size(const queue *q);

/**
 * @brief free the memory allocated by the queue
 *
 * @param q pointer to the queue you want to free
 */
void queue_destroy(queue *q);

#endif
