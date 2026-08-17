/*
 * list - public interface.
 *
 * Before you commit:
 *   1. prefix the include guard below with COLLECTION_, so that it reads
 *      COLLECTION_<MODULE>_H
 *   2. tag every public declaration with COLLECTION_API
 *   3. include what you use: <stdbool.h>, <stddef.h>, <stdint.h>, ...
 *
 * Naming: functions are list_<verb>, the main type is list_t, and macros
 * are COLLECTION_<MODULE>_<NAME>.
 *
 * Errors: return int status codes from collection_error.h, COLLECTION_OK
 * on success and a negative code on failure. Results go in out
 * parameters. Define an E_<MODULE>_<NAME> code counting down from
 * COLLECTION_EMODULE_BASE only when no shared code fits.
 *
 * Ownership: the caller owns the handle. Provide %1$s_init(%1$s_t *)
 * and %1$s_destroy(%1$s_t *) rather than allocating and returning one.
 */

#ifndef COLLECTION_LIST_H
#define COLLECTION_LIST_H
#include "collection_api.h"
#include "collection_error.h"

#include <stddef.h>

/* defined in list.c, so the element layout stays out of the ABI */
struct node;

/**
 * @brief a singly linked list of byte-copied elements
 *
 * The caller owns the storage, so a list_t can live on the stack, in static
 * storage or inside another struct. Initialize it with list_init() before
 * any other call and release its elements with list_destroy().
 *
 * @note the fields are internal. Read the size with list_get_size() and the
 * first node with list_get_first() instead of touching them directly
 */
typedef struct list
{
    struct node *first;
    struct node *last;
    size_t       size;
} list_t;

/**
 * @brief initializes a list, optionally prefilled with a default value
 *
 * @param l pointer to the list to initialize, allocated by the caller
 * @param size number of elements to prefill the list with, 0 for an empty
 * list
 * @param def_val pointer to the value every prefilled element is copied
 * from, ignored when size is 0
 * @param elem_size size of the default value, ignored when size is 0
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l is NULL or if
 * size is non-zero and def_val is NULL, COLLECTION_EINVAL if size is
 * non-zero and elem_size is 0, COLLECTION_ENOMEM if an element could not be
 * allocated
 * @note unless l itself was NULL, a failed call leaves l a valid empty list,
 * so it can be reused or passed to list_destroy() without leaking
 * @note l and def_val must not overlap
 */
COLLECTION_API int list_init(list_t *restrict l, size_t size,
                             const void *restrict def_val, size_t elem_size);

/**
 * @brief frees every element of the list, leaving it empty
 *
 * @param l pointer to the list to empty
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l is NULL
 * @note the list_t belongs to the caller and is not freed. Afterwards l is a
 * valid empty list and can be reused without calling list_init() again
 */
COLLECTION_API int list_destroy(list_t *l);

/**
 * @brief appends a copy of a generic element at the end of the list
 *
 * @param l pointer to the list to append the element to
 * @param value pointer to the data to be stored
 * @param elem_size size of the data to be stored
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l or value is
 * NULL, COLLECTION_EINVAL if elem_size is 0, COLLECTION_ENOMEM if the
 * element could not be allocated
 * @note l and value must not overlap
 */
COLLECTION_API int list_append(list_t *restrict l, const void *restrict value,
                               size_t elem_size);

/**
 * @brief inserts a copy of a generic element at the given position
 *
 * @param l pointer to the list to insert the element into
 * @param index zero based position the new element will take. An index equal
 * to the size of the list appends the element
 * @param value pointer to the data to be stored
 * @param elem_size size of the data to be stored
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l or value is
 * NULL, COLLECTION_EINVAL if elem_size is 0, COLLECTION_ERANGE if index is
 * greater than the size of the list, COLLECTION_ENOMEM if the element could
 * not be allocated
 * @note the element previously at index, and every element after it, shifts
 * one position up. Nothing is overwritten
 * @note l and value must not overlap
 */
COLLECTION_API int list_add_at(list_t *restrict l, size_t index,
                               const void *restrict value, size_t elem_size);

/**
 * @brief removes the first element whose data matches the given value
 *
 * @param l pointer to the list to remove the element from
 * @param value pointer to the data to look for
 * @return int COLLECTION_OK if an element was removed, COLLECTION_ENULL if l
 * or value is NULL, COLLECTION_ENOTFOUND if no element matched
 * @note l and value must not overlap
 */
COLLECTION_API int list_remove(list_t *restrict l, const void *restrict value);

/**
 * @brief removes every element whose data matches the given value
 *
 * @param l pointer to the list to remove the elements from
 * @param value pointer to the data to look for
 * @return int COLLECTION_OK if at least one element was removed,
 * COLLECTION_ENULL if l or value is NULL, COLLECTION_ENOTFOUND if no element
 * matched
 * @note l and value must not overlap
 */
COLLECTION_API int list_remove_all(list_t *restrict l,
                                   const void *restrict value);

/**
 * @brief removes the element at the given position
 *
 * @param l pointer to the list to remove the element from
 * @param index zero based position of the element to remove
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l is NULL,
 * COLLECTION_ERANGE if index is not smaller than the size of the list
 */
COLLECTION_API int list_remove_at(list_t *l, size_t index);

/**
 * @brief finds the position of the first element whose data matches the given
 * value
 *
 * @param l pointer to the list to search
 * @param value pointer to the data to look for
 * @param index out parameter set to the zero based position of the first
 * matching element, untouched on failure
 * @return int COLLECTION_OK if an element matched, COLLECTION_ENULL if l,
 * value or index is NULL, COLLECTION_ENOTFOUND if no element matched
 * @note l, value and index must not overlap
 */
COLLECTION_API int list_find(const list_t *restrict l,
                             const void *restrict value,
                             size_t *restrict index);

/**
 * @brief reads the node at the given position
 *
 * @param l pointer to the list to read from
 * @param index zero based position of the node to read
 * @param node out parameter set to the node at index, untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l or node is
 * NULL, COLLECTION_ERANGE if index is not smaller than the size of the list
 * @note the node belongs to the list and stays valid only until the element
 * is removed or the list is destroyed
 * @note l and node must not overlap
 */
COLLECTION_API int list_at(const list_t *restrict l, size_t index,
                           struct node **restrict node);

/**
 * @brief reads the number of elements in the list
 *
 * @param l pointer to the list you want to get the size of
 * @param size out parameter set to the number of elements, untouched on
 * failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l or size is
 * NULL
 * @note l and size must not overlap
 */
COLLECTION_API int list_get_size(const list_t *restrict l,
                                 size_t *restrict size);

/**
 * @brief reads the first node of the list
 *
 * @param l pointer to the list you want to get the first node of
 * @param first out parameter set to the first node, or to NULL when the list
 * is empty. Untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l or first is
 * NULL
 * @note l and first must not overlap
 */
COLLECTION_API int list_get_first(const list_t *restrict l,
                                  struct node **restrict first);

#endif // COLLECTION_LIST_H
