/*
 * list - implementation.
 *
 * Everything that is not declared in list.h must be `static`: the library
 * is built with hidden visibility and only COLLECTION_API declarations are
 * exported.
 */

#include "list.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct coll_list_node
{
    void                  *data;
    size_t                 size;
    struct coll_list_node *next;
};

static struct coll_list_node *node_create(const void *restrict data,
                                          size_t size)
{
    struct coll_list_node *n = malloc(sizeof(struct coll_list_node));
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

static void node_destroy(struct coll_list_node *n)
{
    if (n == NULL) return;
    free(n->data);
    free(n);
}

/* an element matches only when its stored size equals elem_size, so a value of
   a different size never matches and memcmp never reads beyond either object */
static bool node_matches(const struct coll_list_node *n, const void *value,
                         size_t elem_size)
{
    return n->size == elem_size && memcmp(n->data, value, elem_size) == 0;
}

/* unlinks n from l and destroys it, prev being the node before n
   (NULL when n is the first one) */
static void coll_list_unlink(coll_list *l, struct coll_list_node *prev,
                             struct coll_list_node *n)
{
    if (prev == NULL)
        l->first = n->next;
    else
        prev->next = n->next;

    if (n == l->last)
        l->last = prev;

    node_destroy(n);
    l->size--;
}

/* cuts the chain starting at n after width nodes and returns the head of what
   is left, NULL when the chain holds width nodes or fewer */
static struct coll_list_node *node_split(struct coll_list_node *n, size_t width)
{
    if (n == NULL) return NULL;

    for (size_t i = 1; i < width && n->next != NULL; i++)
        n = n->next;

    struct coll_list_node *rest = n->next;
    n->next = NULL;
    return rest;
}

/* merges two chains that are already sorted into a single sorted one, taking
   from a whenever cmp calls the two heads equivalent, which is what keeps
   equal elements in the order they came in */
static struct coll_list_node *node_merge(struct coll_list_node *a,
                                         struct coll_list_node *b,
                                         coll_list_cmp cmp)
{
    struct coll_list_node *head = NULL;
    struct coll_list_node **tail = &head;

    while (a != NULL && b != NULL)
    {
        if (cmp(a->data, b->data) <= 0)
        {
            *tail = a;
            a = a->next;
        }
        else
        {
            *tail = b;
            b = b->next;
        }

        tail = &(*tail)->next;
    }

    *tail = (a != NULL) ? a : b;
    return head;
}

int coll_list_init(coll_list *restrict l, size_t size,
                   const void *restrict def_val, size_t elem_size)
{
    if (l == NULL) return COLLECTION_ENULL;

    l->first = NULL;
    l->last = NULL;
    l->size = 0;

    if (size == 0) return COLLECTION_OK;

    if (def_val == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;

    for (size_t i = 0; i < size; i++)
    {
        int rc = coll_list_append(l, def_val, elem_size);
        if (rc != COLLECTION_OK)
        {
            /* leaves l empty and reusable rather than half filled */
            coll_list_destroy(l);
            return rc;
        }
    }

    return COLLECTION_OK;
}

int coll_list_destroy(coll_list *l)
{
    if (l == NULL) return COLLECTION_ENULL;

    struct coll_list_node *next = l->first;
    while (next != NULL)
    {
        struct coll_list_node *tmp = next->next;
        node_destroy(next);
        next = tmp;
    }

    l->first = NULL;
    l->last = NULL;
    l->size = 0;

    return COLLECTION_OK;
}

int coll_list_append(coll_list *restrict l, const void *restrict value,
                     size_t elem_size)
{
    if (l == NULL || value == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;

    struct coll_list_node *new = node_create(value, elem_size);
    if (new == NULL) return COLLECTION_ENOMEM;

    if (l->first == NULL)
        l->first = l->last = new;
    else
        l->last = l->last->next = new;

    l->size++;
    return COLLECTION_OK;
}

int coll_list_add_at(coll_list *restrict l, size_t index,
                     const void *restrict value, size_t elem_size)
{
    if (l == NULL || value == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;
    if (index > l->size) return COLLECTION_ERANGE;

    if (index == l->size)
        return coll_list_append(l, value, elem_size);

    struct coll_list_node *new = node_create(value, elem_size);
    if (new == NULL) return COLLECTION_ENOMEM;

    if (index == 0)
    {
        new->next = l->first;
        l->first = new;
    }
    else
    {
        struct coll_list_node *prev = l->first;
        for (size_t i = 1; i < index; i++)
            prev = prev->next;

        new->next = prev->next;
        prev->next = new;
    }

    l->size++;
    return COLLECTION_OK;
}

int coll_list_remove(coll_list *restrict l, const void *restrict value,
                     size_t elem_size)
{
    if (l == NULL || value == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;

    struct coll_list_node *prev = NULL;
    struct coll_list_node *next = l->first;
    while (next != NULL)
    {
        if (node_matches(next, value, elem_size))
        {
            coll_list_unlink(l, prev, next);
            return COLLECTION_OK;
        }

        prev = next;
        next = next->next;
    }

    return COLLECTION_ENOTFOUND;
}

int coll_list_remove_all(coll_list *restrict l, const void *restrict value,
                         size_t elem_size)
{
    if (l == NULL || value == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;

    int rc = COLLECTION_ENOTFOUND;

    struct coll_list_node *prev = NULL;
    struct coll_list_node *next = l->first;
    while (next != NULL)
    {
        struct coll_list_node *tmp = next->next;

        if (node_matches(next, value, elem_size))
        {
            coll_list_unlink(l, prev, next);
            rc = COLLECTION_OK;
        }
        else
            prev = next;

        next = tmp;
    }

    return rc;
}

int coll_list_remove_at(coll_list *l, size_t index)
{
    if (l == NULL) return COLLECTION_ENULL;
    if (index >= l->size) return COLLECTION_ERANGE;

    struct coll_list_node *prev = NULL;
    struct coll_list_node *next = l->first;
    for (size_t i = 0; i < index; i++)
    {
        prev = next;
        next = next->next;
    }

    coll_list_unlink(l, prev, next);
    return COLLECTION_OK;
}

int coll_list_sort(coll_list *l, coll_list_cmp cmp)
{
    if (l == NULL || cmp == NULL) return COLLECTION_ENULL;
    if (l->size < 2) return COLLECTION_OK;

    /* Bottom up merge sort: merge the adjacent runs of width nodes, then
       double width until a single run covers the whole list.

       qsort() cannot do this job. It wants the elements laid out contiguously,
       so it would have to be handed an array of node pointers, which needs an
       allocation that can fail and a comparison of its own wrapping the one
       the caller gave. Merging the chain instead only relinks the nodes, so
       sorting never allocates and cannot fail halfway through. */
    for (size_t width = 1; width < l->size; width *= 2)
    {
        struct coll_list_node *rest = l->first;
        struct coll_list_node *head = NULL;
        struct coll_list_node **tail = &head;

        while (rest != NULL)
        {
            struct coll_list_node *a = rest;
            struct coll_list_node *b = node_split(a, width);
            rest = node_split(b, width);

            *tail = node_merge(a, b, cmp);

            /* walk to the end of what was just merged, so the next pair of
               runs is appended after it */
            while (*tail != NULL)
                tail = &(*tail)->next;
        }

        l->first = head;
    }

    struct coll_list_node *n = l->first;
    while (n->next != NULL)
        n = n->next;
    l->last = n;

    return COLLECTION_OK;
}

int coll_list_reverse(coll_list *l)
{
    if (l == NULL) return COLLECTION_ENULL;
    if (l->size < 2) return COLLECTION_OK;

    /* walks the chain once, pointing every node at the one it came from.
       Relinking in place like this allocates nothing, so reversing cannot
       fail halfway and leave the list in pieces */
    struct coll_list_node *prev = NULL;
    struct coll_list_node *next = l->first;

    while (next != NULL)
    {
        struct coll_list_node *tmp = next->next;
        next->next = prev;
        prev = next;
        next = tmp;
    }

    /* the two ends trade places. The old first node had its next set to NULL
       on the very first pass, so it is already a well formed tail */
    l->last = l->first;
    l->first = prev;

    return COLLECTION_OK;
}

int coll_list_find(const coll_list *restrict l, const void *restrict value,
                   size_t elem_size, size_t *restrict index)
{
    if (l == NULL || value == NULL || index == NULL) return COLLECTION_ENULL;
    if (elem_size == 0) return COLLECTION_EINVAL;

    size_t i = 0;
    struct coll_list_node *next = l->first;
    while (next != NULL)
    {
        if (node_matches(next, value, elem_size))
        {
            *index = i;
            return COLLECTION_OK;
        }

        next = next->next;
        i++;
    }

    return COLLECTION_ENOTFOUND;
}

int coll_list_at(const coll_list *restrict l, size_t index,
                 struct coll_list_node **restrict node)
{
    if (l == NULL || node == NULL) return COLLECTION_ENULL;
    if (index >= l->size) return COLLECTION_ERANGE;

    struct coll_list_node *next = l->first;
    for (size_t i = 0; i < index; i++)
        next = next->next;

    *node = next;
    return COLLECTION_OK;
}

int coll_list_get_size(const coll_list *restrict l, size_t *restrict size)
{
    if (l == NULL || size == NULL) return COLLECTION_ENULL;

    *size = l->size;
    return COLLECTION_OK;
}

int coll_list_get_first(const coll_list *restrict l,
                        struct coll_list_node **restrict first)
{
    if (l == NULL || first == NULL) return COLLECTION_ENULL;

    *first = l->first;
    return COLLECTION_OK;
}
