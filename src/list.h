/*
 * list - public interface.
 *
 * Naming: every exported name carries the coll_ prefix, so functions are
 * coll_list_<verb> and the main type is coll_list, with no _t suffix: POSIX
 * reserves that one. Macros are COLLECTION_<MODULE>_<NAME>: they never reach
 * the linker, so they keep the longer prefix.
 *
 * Errors: return int status codes from collection_error.h, COLLECTION_OK
 * on success and a negative code on failure. Results go in out
 * parameters. Define an E_<MODULE>_<NAME> code counting down from
 * COLLECTION_EMODULE_BASE only when no shared code fits.
 *
 * Ownership: the caller owns the handle. Provide coll_list_init(coll_list *)
 * and coll_list_destroy(coll_list *) rather than allocating and returning
 * one.
 */

#ifndef COLLECTION_LIST_H
#define COLLECTION_LIST_H
#include "collection_api.h"
#include "collection_error.h"

#include <stddef.h>

/* defined in list.c, so the element layout stays out of the ABI. Every module
   names its node tag after itself, so the tags stay distinct once every header
   lands in the same translation unit through <collection.h> */
struct coll_list_node;

/**
 * @brief a singly linked list of byte-copied elements
 *
 * The caller owns the storage, so a coll_list can live on the stack, in
 * static storage or inside another struct. Initialize it with coll_list_init()
 * before any other call and release its elements with coll_list_destroy().
 *
 * @note the fields are internal. Read the size with coll_list_get_size() and
 * the first node with coll_list_get_first() instead of touching them directly
 */
typedef struct coll_list
{
    struct coll_list_node *first;
    struct coll_list_node *last;
    size_t                 size;
} coll_list;

/**
 * @brief comparison deciding the order of two elements of a list
 *
 * Handed to coll_list_sort() and called with the data pointers of the two
 * elements being compared, in the same spirit as the callback qsort() takes.
 * Return a negative value when a belongs before b, a positive value when it
 * belongs after it, and 0 when the two are equivalent.
 *
 * @note the elements belong to the list: a comparison must not modify them,
 * and must not touch the list itself
 */
typedef int (*coll_list_cmp)(const void *a, const void *b);

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
 * so it can be reused or passed to coll_list_destroy() without leaking
 * @note l and def_val must not overlap
 */
COLLECTION_API int coll_list_init(coll_list *restrict l, size_t size,
                                  const void *restrict def_val,
                                  size_t elem_size);

/**
 * @brief frees every element of the list, leaving it empty
 *
 * @param l pointer to the list to empty
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l is NULL
 * @note the coll_list belongs to the caller and is not freed. Afterwards l is
 * a valid empty list and can be reused without calling coll_list_init() again
 */
COLLECTION_API int coll_list_destroy(coll_list *l);

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
COLLECTION_API int coll_list_append(coll_list *restrict l,
                                    const void *restrict value,
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
COLLECTION_API int coll_list_add_at(coll_list *restrict l, size_t index,
                                    const void *restrict value,
                                    size_t elem_size);

/**
 * @brief removes the first element whose data matches the given value
 *
 * @param l pointer to the list to remove the element from
 * @param value pointer to the data to look for
 * @param elem_size size of the data to look for. An element matches only when
 * its own size equals this one, so a value of a different size never matches
 * and is never read beyond
 * @return int COLLECTION_OK if an element was removed, COLLECTION_ENULL if l
 * or value is NULL, COLLECTION_EINVAL if elem_size is 0, COLLECTION_ENOTFOUND
 * if no element matched
 * @note l and value must not overlap
 */
COLLECTION_API int coll_list_remove(coll_list *restrict l,
                                    const void *restrict value,
                                    size_t elem_size);

/**
 * @brief removes every element whose data matches the given value
 *
 * @param l pointer to the list to remove the elements from
 * @param value pointer to the data to look for
 * @param elem_size size of the data to look for. An element matches only when
 * its own size equals this one, so a value of a different size never matches
 * and is never read beyond
 * @return int COLLECTION_OK if at least one element was removed,
 * COLLECTION_ENULL if l or value is NULL, COLLECTION_EINVAL if elem_size is 0,
 * COLLECTION_ENOTFOUND if no element matched
 * @note l and value must not overlap
 */
COLLECTION_API int coll_list_remove_all(coll_list *restrict l,
                                        const void *restrict value,
                                        size_t elem_size);

/**
 * @brief removes the element at the given position
 *
 * @param l pointer to the list to remove the element from
 * @param index zero based position of the element to remove
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l is NULL,
 * COLLECTION_ERANGE if index is not smaller than the size of the list
 */
COLLECTION_API int coll_list_remove_at(coll_list *l, size_t index);

/**
 * @brief sorts the list in place, ordering its elements with the given
 * comparison
 *
 * @param l pointer to the list to sort
 * @param cmp comparison deciding the order of two elements, called with the
 * data pointers of the elements being compared
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l or cmp is NULL
 * @note the sort is stable: elements cmp calls equivalent keep the relative
 * order they had before the call
 * @note a list of fewer than two elements is already sorted, so it succeeds
 * without cmp ever being called
 * @note the nodes are relinked rather than reallocated, so an element keeps its
 * address and only changes position. Any node held from an earlier
 * coll_list_at() or coll_list_get_first() stays valid but no longer sits at the
 * index it came from
 * @note cmp is handed no size, so it has to know the layout of the elements on
 * its own. A list holding elements of different sizes can only be sorted by a
 * comparison that can tell them apart by itself
 */
COLLECTION_API int coll_list_sort(coll_list *l, coll_list_cmp cmp);

/**
 * @brief reverses the list in place, so the element that was last comes first
 *
 * @param l pointer to the list to reverse
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if l is NULL
 * @note a list of fewer than two elements is already its own reverse, so it
 * succeeds without anything being touched
 * @note the nodes are relinked rather than reallocated, so an element keeps its
 * address and only changes position. Any node held from an earlier
 * coll_list_at() or coll_list_get_first() stays valid but no longer sits at the
 * index it came from
 * @note reversing allocates nothing, so it cannot fail partway and leave the
 * list half turned around
 * @note this is not the same as sorting with the opposite comparison: it
 * turns around the order the list is actually in, whatever that order is
 */
COLLECTION_API int coll_list_reverse(coll_list *l);

/**
 * @brief finds the position of the first element whose data matches the given
 * value
 *
 * @param l pointer to the list to search
 * @param value pointer to the data to look for
 * @param elem_size size of the data to look for. An element matches only when
 * its own size equals this one, so a value of a different size never matches
 * and is never read beyond
 * @param index out parameter set to the zero based position of the first
 * matching element, untouched on failure
 * @return int COLLECTION_OK if an element matched, COLLECTION_ENULL if l,
 * value or index is NULL, COLLECTION_EINVAL if elem_size is 0,
 * COLLECTION_ENOTFOUND if no element matched
 * @note l, value and index must not overlap
 */
COLLECTION_API int coll_list_find(const coll_list *restrict l,
                                  const void *restrict value, size_t elem_size,
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
COLLECTION_API int coll_list_at(const coll_list *restrict l, size_t index,
                                struct coll_list_node **restrict node);

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
COLLECTION_API int coll_list_get_size(const coll_list *restrict l,
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
COLLECTION_API int coll_list_get_first(const coll_list *restrict l,
                                       struct coll_list_node **restrict first);

#endif // COLLECTION_LIST_H
