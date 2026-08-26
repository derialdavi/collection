/*
 * stack - public interface.
 *
 * Naming: every exported name carries the coll_ prefix, so functions are
 * coll_stack_<verb> and the main type is coll_stack, with no _t suffix: POSIX
 * reserves that one. Macros are COLLECTION_<MODULE>_<NAME>: they never reach
 * the linker, so they keep the longer prefix.
 *
 * Errors: return int status codes from collection_error.h, COLLECTION_OK
 * on success and a negative code on failure. Results go in out
 * parameters. Define an E_<MODULE>_<NAME> code counting down from
 * COLLECTION_EMODULE_BASE only when no shared code fits.
 *
 * Ownership: the caller owns the handle. Provide
 * coll_stack_init(coll_stack *) and coll_stack_destroy(coll_stack *)
 * rather than allocating and returning one.
 */

#ifndef COLLECTION_STACK_H
#define COLLECTION_STACK_H
#include "collection_api.h"
#include "collection_error.h"

#include <stdbool.h>
#include <stddef.h>

/* defined in stack.c, so the element layout stays out of the ABI. Every module
   names its node tag after itself, so the tags stay distinct once every header
   lands in the same translation unit through <collection.h> */
struct coll_stack_node;

/**
 * @brief a last in first out stack of byte-copied elements
 *
 * The caller owns the storage, so a coll_stack can live on the stack, in
 * static storage or inside another struct. Initialize it with coll_stack_init()
 * before any other call and release its elements with coll_stack_destroy().
 *
 * Elements enter and leave at the same end, the top: coll_stack_push() puts one
 * there and coll_stack_pop() takes the one that arrived most recently, so the
 * element pushed last is always the one that comes out next.
 *
 * @note the fields are internal. Read the number of elements with
 * coll_stack_get_size() instead of touching them directly
 */
typedef struct coll_stack
{
    struct coll_stack_node *top;
    size_t                  size;
} coll_stack;

/**
 * @brief initializes a stack, optionally prefilled with a default value
 *
 * @param s pointer to the stack to initialize, allocated by the caller
 * @param size number of elements to prefill the stack with, 0 for an empty
 * stack
 * @param def_val pointer to the value every prefilled element is copied from,
 * ignored when size is 0
 * @param elem_size size of the default value, ignored when size is 0
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if s is NULL or if
 * size is non-zero and def_val is NULL, COLLECTION_EINVAL if size is non-zero
 * and elem_size is 0, COLLECTION_ENOMEM if an element could not be allocated
 * @note unless s itself was NULL, a failed call leaves s a valid empty stack,
 * so it can be reused or passed to coll_stack_destroy() without leaking
 * @note s and def_val must not overlap
 */
COLLECTION_API int coll_stack_init(coll_stack *restrict s, size_t size,
                                   const void *restrict def_val,
                                   size_t elem_size);

/**
 * @brief frees every element of the stack, leaving it empty
 *
 * @param s pointer to the stack to empty
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if s is NULL
 * @note the coll_stack belongs to the caller and is not freed. Afterwards s
 * is a valid empty stack and can be reused without calling coll_stack_init()
 * again
 */
COLLECTION_API int coll_stack_destroy(coll_stack *s);

/**
 * @brief puts a copy of a generic element on top of the stack
 *
 * @param s pointer to the stack to push the element onto
 * @param value pointer to the data to be stored
 * @param elem_size size of the data to be stored
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if s or value is
 * NULL, COLLECTION_EINVAL if elem_size is 0, COLLECTION_ENOMEM if the element
 * could not be allocated
 * @note s and value must not overlap
 */
COLLECTION_API int coll_stack_push(coll_stack *restrict s,
                                   const void *restrict value,
                                   size_t elem_size);

/**
 * @brief removes the element on top of the stack and copies it out
 *
 * @param s pointer to the stack to take the element from
 * @param value pointer to the storage the element is copied into
 * @param elem_size size of that storage. It must equal the size the element
 * was stored with, so an element is never truncated and is never read beyond
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if s or value is
 * NULL, COLLECTION_EINVAL if elem_size is 0 or does not match the size of the
 * top element, COLLECTION_ENOTFOUND if the stack is empty
 * @note the element is only removed when the call succeeds: a size mismatch
 * leaves the stack exactly as it was, so the caller can ask
 * coll_stack_peek_size() for the right size and try again
 * @note s and value must not overlap
 */
COLLECTION_API int coll_stack_pop(coll_stack *restrict s,
                                  void *restrict value, size_t elem_size);

/**
 * @brief copies out the element on top of the stack without removing it
 *
 * @param s pointer to the stack to read from
 * @param value pointer to the storage the element is copied into
 * @param elem_size size of that storage. It must equal the size the element
 * was stored with, so an element is never truncated and is never read beyond
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if s or value is
 * NULL, COLLECTION_EINVAL if elem_size is 0 or does not match the size of the
 * top element, COLLECTION_ENOTFOUND if the stack is empty
 * @note s and value must not overlap
 */
COLLECTION_API int coll_stack_peek(const coll_stack *restrict s,
                                   void *restrict value, size_t elem_size);

/**
 * @brief reads the size of the element on top of the stack
 *
 * @param s pointer to the stack to read from
 * @param elem_size out parameter set to the size the top element was stored
 * with, untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if s or elem_size is
 * NULL, COLLECTION_ENOTFOUND if the stack is empty
 * @note a stack can hold elements of different sizes. This is how a caller
 * that does not already know the layout finds out how much storage
 * coll_stack_pop() and coll_stack_peek() expect
 * @note s and elem_size must not overlap
 */
COLLECTION_API int coll_stack_peek_size(const coll_stack *restrict s,
                                        size_t *restrict elem_size);

/**
 * @brief reads the number of elements in the stack
 *
 * @param s pointer to the stack you want to get the size of
 * @param size out parameter set to the number of elements, untouched on
 * failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if s or size is NULL
 * @note s and size must not overlap
 */
COLLECTION_API int coll_stack_get_size(const coll_stack *restrict s,
                                       size_t *restrict size);

/**
 * @brief tells whether the stack holds no elements
 *
 * @param s pointer to the stack to inspect
 * @param empty out parameter set to true when the stack holds no elements and
 * to false otherwise, untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if s or empty is NULL
 * @note s and empty must not overlap
 */
COLLECTION_API int coll_stack_is_empty(const coll_stack *restrict s,
                                       bool *restrict empty);

#endif // COLLECTION_STACK_H
