/*
 * stack - implementation.
 *
 * Everything that is not declared in stack.h must be `static`: the library
 * is built with hidden visibility and only COLLECTION_API declarations are
 * exported.
 */

#include "stack.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct stack_node
{
    void              *data;
    size_t             size;
    struct stack_node *next;
};

static struct stack_node *node_create(const void *restrict data, size_t size)
{
    struct stack_node *n = malloc(sizeof(struct stack_node));
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

static void node_destroy(struct stack_node *n)
{
    if (n == NULL) return;
    free(n->data);
    free(n);
}

/* copies the top element of s into value, which must be exactly as large as
   the element was stored with, so nothing is truncated and memcpy never reads
   beyond either object. Shared by stack_pop() and stack_peek() */
static int stack_copy_top(const cstack_t *restrict s, void *restrict value,
                          size_t elem_size)
{
    if (s == NULL || value == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;
    if (s->top == NULL) return COLLECTION_ENOTFOUND;
    if (s->top->size != elem_size) return COLLECTION_EINVAL;

    memcpy(value, s->top->data, elem_size);
    return COLLECTION_OK;
}

int stack_init(cstack_t *restrict s, size_t size, const void *restrict def_val,
               size_t elem_size)
{
    if (s == NULL) return COLLECTION_ENULL;

    s->top = NULL;
    s->size = 0;

    if (size == 0) return COLLECTION_OK;

    if (def_val == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;

    for (size_t i = 0; i < size; i++)
    {
        int rc = stack_push(s, def_val, elem_size);
        if (rc != COLLECTION_OK)
        {
            /* leaves s empty and reusable rather than half filled */
            stack_destroy(s);
            return rc;
        }
    }

    return COLLECTION_OK;
}

int stack_destroy(cstack_t *s)
{
    if (s == NULL) return COLLECTION_ENULL;

    struct stack_node *next = s->top;
    while (next != NULL)
    {
        struct stack_node *tmp = next->next;
        node_destroy(next);
        next = tmp;
    }

    s->top = NULL;
    s->size = 0;

    return COLLECTION_OK;
}

int stack_push(cstack_t *restrict s, const void *restrict value,
               size_t elem_size)
{
    if (s == NULL || value == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;

    struct stack_node *new = node_create(value, elem_size);
    if (new == NULL) return COLLECTION_ENOMEM;

    /* the element that was on top is now the one below it */
    new->next = s->top;
    s->top = new;

    s->size++;
    return COLLECTION_OK;
}

int stack_pop(cstack_t *restrict s, void *restrict value, size_t elem_size)
{
    /* the copy happens first, so a rejected call removes nothing */
    int rc = stack_copy_top(s, value, elem_size);
    if (rc != COLLECTION_OK) return rc;

    struct stack_node *top = s->top;

    s->top = top->next;

    node_destroy(top);
    s->size--;

    return COLLECTION_OK;
}

int stack_peek(const cstack_t *restrict s, void *restrict value,
               size_t elem_size)
{
    return stack_copy_top(s, value, elem_size);
}

int stack_peek_size(const cstack_t *restrict s, size_t *restrict elem_size)
{
    if (s == NULL || elem_size == NULL) return COLLECTION_ENULL;
    if (s->top == NULL) return COLLECTION_ENOTFOUND;

    *elem_size = s->top->size;
    return COLLECTION_OK;
}

int stack_get_size(const cstack_t *restrict s, size_t *restrict size)
{
    if (s == NULL || size == NULL) return COLLECTION_ENULL;

    *size = s->size;
    return COLLECTION_OK;
}

int stack_is_empty(const cstack_t *restrict s, bool *restrict empty)
{
    if (s == NULL || empty == NULL) return COLLECTION_ENULL;

    *empty = (s->size == 0);
    return COLLECTION_OK;
}
