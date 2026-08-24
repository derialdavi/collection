/*
 * stack - public interface.
 *
 * Naming: functions are stack_<verb> and macros are
 * COLLECTION_<MODULE>_<NAME>. The main type is cstack_t rather than stack_t,
 * the one place this module departs from the <module>_t convention: POSIX
 * already defines stack_t in <signal.h> for sigaltstack, and on some platforms
 * <stdlib.h> drags that in, so a stack_t here would clash in any translation
 * unit that includes both.
 *
 * Errors: return int status codes from collection_error.h, COLLECTION_OK
 * on success and a negative code on failure. Results go in out
 * parameters. Define an E_<MODULE>_<NAME> code counting down from
 * COLLECTION_EMODULE_BASE only when no shared code fits.
 *
 * Ownership: the caller owns the handle. Provide stack_init(cstack_t *)
 * and stack_destroy(cstack_t *) rather than allocating and returning one.
 */

#ifndef COLLECTION_STACK_H
#define COLLECTION_STACK_H
#include "collection_api.h"
#include "collection_error.h"

#include <stdbool.h>
#include <stddef.h>

/* defined in stack.c, so the element layout stays out of the ABI. The tag is
   stack_node rather than node because list.h declares a struct node of its
   own, and every header ends up in the same translation unit through
   <collection.h> */
struct stack_node;

/**
 * @brief a last in first out stack of byte-copied elements
 *
 * The caller owns the storage, so a cstack_t can live on the stack, in static
 * storage or inside another struct. Initialize it with stack_init() before any
 * other call and release its elements with stack_destroy().
 *
 * Elements enter and leave at the same end, the top: stack_push() puts one
 * there and stack_pop() takes the one that arrived most recently, so the
 * element pushed last is always the one that comes out next.
 *
 * @note the fields are internal. Read the number of elements with
 * stack_get_size() instead of touching them directly
 * @note the type is cstack_t, not stack_t: that name belongs to POSIX
 * <signal.h>. The struct tag and every function keep the plain stack name
 */
typedef struct stack
{
    struct stack_node *top;
    size_t             size;
} cstack_t;

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
 * so it can be reused or passed to stack_destroy() without leaking
 * @note s and def_val must not overlap
 */
COLLECTION_API int stack_init(cstack_t *restrict s, size_t size,
                              const void *restrict def_val, size_t elem_size);

/**
 * @brief frees every element of the stack, leaving it empty
 *
 * @param s pointer to the stack to empty
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if s is NULL
 * @note the cstack_t belongs to the caller and is not freed. Afterwards s is a
 * valid empty stack and can be reused without calling stack_init() again
 */
COLLECTION_API int stack_destroy(cstack_t *s);

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
COLLECTION_API int stack_push(cstack_t *restrict s, const void *restrict value,
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
 * stack_peek_size() for the right size and try again
 * @note s and value must not overlap
 */
COLLECTION_API int stack_pop(cstack_t *restrict s, void *restrict value,
                             size_t elem_size);

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
COLLECTION_API int stack_peek(const cstack_t *restrict s, void *restrict value,
                              size_t elem_size);

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
 * stack_pop() and stack_peek() expect
 * @note s and elem_size must not overlap
 */
COLLECTION_API int stack_peek_size(const cstack_t *restrict s,
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
COLLECTION_API int stack_get_size(const cstack_t *restrict s,
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
COLLECTION_API int stack_is_empty(const cstack_t *restrict s,
                                  bool *restrict empty);

#endif // COLLECTION_STACK_H
