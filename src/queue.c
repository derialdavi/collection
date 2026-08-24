/*
 * queue - implementation.
 *
 * Everything that is not declared in queue.h must be `static`: the library
 * is built with hidden visibility and only COLLECTION_API declarations are
 * exported.
 */

#include "queue.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct queue_node
{
    void              *data;
    size_t             size;
    struct queue_node *next;
};

static struct queue_node *node_create(const void *restrict data, size_t size)
{
    struct queue_node *n = malloc(sizeof(struct queue_node));
    if (n == NULL) return NULL;

    n->data = malloc(size);
    if (n->data == NULL)
    {
        free(n);
        return NULL;
    }

    memcpy(n->data, data, size);
    n->size = size;
    n->next = NULL;

    return n;
}

static void node_destroy(struct queue_node *n)
{
    if (n == NULL) return;
    free(n->data);
    free(n);
}

/* copies the front element of q into value, which must be exactly as large as
   the element was stored with, so nothing is truncated and memcpy never reads
   beyond either object. Shared by queue_dequeue() and queue_peek() */
static int queue_copy_front(const queue_t *restrict q, void *restrict value,
                            size_t elem_size)
{
    if (q == NULL || value == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;
    if (q->front == NULL) return COLLECTION_ENOTFOUND;
    if (q->front->size != elem_size) return COLLECTION_EINVAL;

    memcpy(value, q->front->data, elem_size);
    return COLLECTION_OK;
}

int queue_init(queue_t *restrict q, size_t size, const void *restrict def_val,
               size_t elem_size)
{
    if (q == NULL) return COLLECTION_ENULL;

    q->front = NULL;
    q->back = NULL;
    q->size = 0;

    if (size == 0) return COLLECTION_OK;

    if (def_val == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;

    for (size_t i = 0; i < size; i++)
    {
        int rc = queue_enqueue(q, def_val, elem_size);
        if (rc != COLLECTION_OK)
        {
            /* leaves q empty and reusable rather than half filled */
            queue_destroy(q);
            return rc;
        }
    }

    return COLLECTION_OK;
}

int queue_destroy(queue_t *q)
{
    if (q == NULL) return COLLECTION_ENULL;

    struct queue_node *next = q->front;
    while (next != NULL)
    {
        struct queue_node *tmp = next->next;
        node_destroy(next);
        next = tmp;
    }

    q->front = NULL;
    q->back = NULL;
    q->size = 0;

    return COLLECTION_OK;
}

int queue_enqueue(queue_t *restrict q, const void *restrict value,
                  size_t elem_size)
{
    if (q == NULL || value == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;

    struct queue_node *new = node_create(value, elem_size);
    if (new == NULL) return COLLECTION_ENOMEM;

    if (q->front == NULL)
        q->front = q->back = new;
    else
        q->back = q->back->next = new;

    q->size++;
    return COLLECTION_OK;
}

int queue_dequeue(queue_t *restrict q, void *restrict value, size_t elem_size)
{
    /* the copy happens first, so a rejected call removes nothing */
    int rc = queue_copy_front(q, value, elem_size);
    if (rc != COLLECTION_OK) return rc;

    struct queue_node *front = q->front;

    q->front = front->next;
    if (q->front == NULL) q->back = NULL;

    node_destroy(front);
    q->size--;

    return COLLECTION_OK;
}

int queue_peek(const queue_t *restrict q, void *restrict value,
               size_t elem_size)
{
    return queue_copy_front(q, value, elem_size);
}

int queue_peek_size(const queue_t *restrict q, size_t *restrict elem_size)
{
    if (q == NULL || elem_size == NULL) return COLLECTION_ENULL;
    if (q->front == NULL) return COLLECTION_ENOTFOUND;

    *elem_size = q->front->size;
    return COLLECTION_OK;
}

int queue_get_size(const queue_t *restrict q, size_t *restrict size)
{
    if (q == NULL || size == NULL) return COLLECTION_ENULL;

    *size = q->size;
    return COLLECTION_OK;
}

int queue_is_empty(const queue_t *restrict q, bool *restrict empty)
{
    if (q == NULL || empty == NULL) return COLLECTION_ENULL;

    *empty = (q->size == 0);
    return COLLECTION_OK;
}
