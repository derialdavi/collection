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

struct coll_queue_node
{
    void                   *data;
    size_t                  size;
    struct coll_queue_node *next;
};

static struct coll_queue_node *node_create(const void *restrict data,
                                           size_t size)
{
    struct coll_queue_node *n = malloc(sizeof(struct coll_queue_node));
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

static void node_destroy(struct coll_queue_node *n)
{
    if (n == NULL) return;
    free(n->data);
    free(n);
}

/* copies the front element of q into value, which must be exactly as large as
   the element was stored with, so nothing is truncated and memcpy never reads
   beyond either object. Shared by coll_queue_dequeue() and coll_queue_peek() */
static int coll_queue_copy_front(const coll_queue *restrict q,
                                 void *restrict value, size_t elem_size)
{
    if (q == NULL || value == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;
    if (q->front == NULL) return COLLECTION_ENOTFOUND;
    if (q->front->size != elem_size) return COLLECTION_EINVAL;

    memcpy(value, q->front->data, elem_size);
    return COLLECTION_OK;
}

int coll_queue_init(coll_queue *restrict q, size_t size,
                    const void *restrict def_val, size_t elem_size)
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
        int rc = coll_queue_enqueue(q, def_val, elem_size);
        if (rc != COLLECTION_OK)
        {
            /* leaves q empty and reusable rather than half filled */
            coll_queue_destroy(q);
            return rc;
        }
    }

    return COLLECTION_OK;
}

int coll_queue_destroy(coll_queue *q)
{
    if (q == NULL) return COLLECTION_ENULL;

    struct coll_queue_node *next = q->front;
    while (next != NULL)
    {
        struct coll_queue_node *tmp = next->next;
        node_destroy(next);
        next = tmp;
    }

    q->front = NULL;
    q->back = NULL;
    q->size = 0;

    return COLLECTION_OK;
}

int coll_queue_enqueue(coll_queue *restrict q, const void *restrict value,
                       size_t elem_size)
{
    if (q == NULL || value == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;

    struct coll_queue_node *new = node_create(value, elem_size);
    if (new == NULL) return COLLECTION_ENOMEM;

    if (q->front == NULL)
        q->front = q->back = new;
    else
        q->back = q->back->next = new;

    q->size++;
    return COLLECTION_OK;
}

int coll_queue_dequeue(coll_queue *restrict q, void *restrict value,
                       size_t elem_size)
{
    /* the copy happens first, so a rejected call removes nothing */
    int rc = coll_queue_copy_front(q, value, elem_size);
    if (rc != COLLECTION_OK) return rc;

    struct coll_queue_node *front = q->front;

    q->front = front->next;
    if (q->front == NULL) q->back = NULL;

    node_destroy(front);
    q->size--;

    return COLLECTION_OK;
}

int coll_queue_peek(const coll_queue *restrict q, void *restrict value,
                    size_t elem_size)
{
    return coll_queue_copy_front(q, value, elem_size);
}

int coll_queue_peek_size(const coll_queue *restrict q,
                         size_t *restrict elem_size)
{
    if (q == NULL || elem_size == NULL) return COLLECTION_ENULL;
    if (q->front == NULL) return COLLECTION_ENOTFOUND;

    *elem_size = q->front->size;
    return COLLECTION_OK;
}

int coll_queue_get_size(const coll_queue *restrict q, size_t *restrict size)
{
    if (q == NULL || size == NULL) return COLLECTION_ENULL;

    *size = q->size;
    return COLLECTION_OK;
}

int coll_queue_is_empty(const coll_queue *restrict q, bool *restrict empty)
{
    if (q == NULL || empty == NULL) return COLLECTION_ENULL;

    *empty = (q->size == 0);
    return COLLECTION_OK;
}
