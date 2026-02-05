#include "queue.h"

#include <string.h>

void free_nodes(queue_node *);

queue *queue_create()
{
    queue *q = malloc(sizeof(queue));
    if (q == NULL) return NULL;

    q->head = q->tail = NULL;
    q->size = 0;

    return q;
}

bool queue_enque(queue *q, const void *data, const size_t size)
{
    if (q == NULL || data == NULL || size <= 0)
        return false;

    queue_node *new_node = malloc(sizeof(queue_node));
    if (new_node == NULL) return false;

    new_node->data = malloc(size);
    if (new_node->data == NULL) return false;

    memcpy(new_node->data, data, size);
    new_node->size = size;
    new_node->next = NULL;

    if (q->head == NULL)
        q->head = q->tail = new_node;
    else
        q->tail = q->tail->next = new_node;

    q->size++;
    return true;
}

void *queue_deque(queue *q)
{
    if (q == NULL || q->size <= 0)
        return NULL;

    void *out = malloc(q->head->size);
    if (out == NULL) return NULL;
    memcpy(out, q->head->data, q->head->size);

    queue_node *tmp = q->head;
    q->head = tmp->next;
    if (q->head == NULL)
        q->tail = NULL;

    free(tmp->data);
    free(tmp);
    q->size--;

    return out;
}

int queue_size(const queue *q)
{
    if (q == NULL) return -1;
    return q->size;
}

void queue_destroy(queue *q)
{
    if (q == NULL) return;

    free_nodes(q->head);
    free(q);
}

void free_nodes(queue_node *n)
{
    if (n == NULL) return;

    if (n->next == NULL)
    {
        free(n->data);
        free(n);
        return;
    }

    free_nodes(n->next);
    free(n->data);
    free(n);
}
