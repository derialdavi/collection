/*
 * queue - public interface.
 *
 * Naming: functions are queue_<verb>, the main type is queue_t, and macros
 * are COLLECTION_<MODULE>_<NAME>.
 *
 * Errors: return int status codes from collection_error.h, COLLECTION_OK
 * on success and a negative code on failure. Results go in out
 * parameters. Define an E_<MODULE>_<NAME> code counting down from
 * COLLECTION_EMODULE_BASE only when no shared code fits.
 *
 * Ownership: the caller owns the handle. Provide queue_init(queue_t *)
 * and queue_destroy(queue_t *) rather than allocating and returning one.
 */

#ifndef COLLECTION_QUEUE_H
#define COLLECTION_QUEUE_H
#include "collection_api.h"
#include "collection_error.h"

#include <stdbool.h>
#include <stddef.h>

/* defined in queue.c, so the element layout stays out of the ABI. The tag is
   queue_node rather than node because list.h declares a struct node of its
   own, and both headers end up in the same translation unit through
   <collection.h> */
struct queue_node;

/**
 * @brief a first in first out queue of byte-copied elements
 *
 * The caller owns the storage, so a queue_t can live on the stack, in static
 * storage or inside another struct. Initialize it with queue_init() before any
 * other call and release its elements with queue_destroy().
 *
 * Elements enter at the back with queue_enqueue() and leave from the front
 * with queue_dequeue(), so the one that has waited longest is always the one
 * that comes out next.
 *
 * @note the fields are internal. Read the number of elements with
 * queue_get_size() instead of touching them directly
 */
typedef struct queue
{
    struct queue_node *front;
    struct queue_node *back;
    size_t             size;
} queue_t;

/**
 * @brief initializes a queue, optionally prefilled with a default value
 *
 * @param q pointer to the queue to initialize, allocated by the caller
 * @param size number of elements to prefill the queue with, 0 for an empty
 * queue
 * @param def_val pointer to the value every prefilled element is copied from,
 * ignored when size is 0
 * @param elem_size size of the default value, ignored when size is 0
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if q is NULL or if
 * size is non-zero and def_val is NULL, COLLECTION_EINVAL if size is non-zero
 * and elem_size is 0, COLLECTION_ENOMEM if an element could not be allocated
 * @note unless q itself was NULL, a failed call leaves q a valid empty queue,
 * so it can be reused or passed to queue_destroy() without leaking
 * @note q and def_val must not overlap
 */
COLLECTION_API int queue_init(queue_t *restrict q, size_t size,
                              const void *restrict def_val, size_t elem_size);

/**
 * @brief frees every element of the queue, leaving it empty
 *
 * @param q pointer to the queue to empty
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if q is NULL
 * @note the queue_t belongs to the caller and is not freed. Afterwards q is a
 * valid empty queue and can be reused without calling queue_init() again
 */
COLLECTION_API int queue_destroy(queue_t *q);

/**
 * @brief appends a copy of a generic element at the back of the queue
 *
 * @param q pointer to the queue to append the element to
 * @param value pointer to the data to be stored
 * @param elem_size size of the data to be stored
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if q or value is
 * NULL, COLLECTION_EINVAL if elem_size is 0, COLLECTION_ENOMEM if the element
 * could not be allocated
 * @note q and value must not overlap
 */
COLLECTION_API int queue_enqueue(queue_t *restrict q,
                                 const void *restrict value, size_t elem_size);

/**
 * @brief removes the element at the front of the queue and copies it out
 *
 * @param q pointer to the queue to take the element from
 * @param value pointer to the storage the element is copied into
 * @param elem_size size of that storage. It must equal the size the element
 * was stored with, so an element is never truncated and is never read beyond
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if q or value is
 * NULL, COLLECTION_EINVAL if elem_size is 0 or does not match the size of the
 * front element, COLLECTION_ENOTFOUND if the queue is empty
 * @note the element is only removed when the call succeeds: a size mismatch
 * leaves the queue exactly as it was, so the caller can ask
 * queue_peek_size() for the right size and try again
 * @note q and value must not overlap
 */
COLLECTION_API int queue_dequeue(queue_t *restrict q, void *restrict value,
                                 size_t elem_size);

/**
 * @brief copies out the element at the front of the queue without removing it
 *
 * @param q pointer to the queue to read from
 * @param value pointer to the storage the element is copied into
 * @param elem_size size of that storage. It must equal the size the element
 * was stored with, so an element is never truncated and is never read beyond
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if q or value is
 * NULL, COLLECTION_EINVAL if elem_size is 0 or does not match the size of the
 * front element, COLLECTION_ENOTFOUND if the queue is empty
 * @note q and value must not overlap
 */
COLLECTION_API int queue_peek(const queue_t *restrict q, void *restrict value,
                              size_t elem_size);

/**
 * @brief reads the size of the element at the front of the queue
 *
 * @param q pointer to the queue to read from
 * @param elem_size out parameter set to the size the front element was stored
 * with, untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if q or elem_size is
 * NULL, COLLECTION_ENOTFOUND if the queue is empty
 * @note a queue can hold elements of different sizes. This is how a caller
 * that does not already know the layout finds out how much storage
 * queue_dequeue() and queue_peek() expect
 * @note q and elem_size must not overlap
 */
COLLECTION_API int queue_peek_size(const queue_t *restrict q,
                                   size_t *restrict elem_size);

/**
 * @brief reads the number of elements in the queue
 *
 * @param q pointer to the queue you want to get the size of
 * @param size out parameter set to the number of elements, untouched on
 * failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if q or size is NULL
 * @note q and size must not overlap
 */
COLLECTION_API int queue_get_size(const queue_t *restrict q,
                                  size_t *restrict size);

/**
 * @brief tells whether the queue holds no elements
 *
 * @param q pointer to the queue to inspect
 * @param empty out parameter set to true when the queue holds no elements and
 * to false otherwise, untouched on failure
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if q or empty is NULL
 * @note q and empty must not overlap
 */
COLLECTION_API int queue_is_empty(const queue_t *restrict q,
                                  bool *restrict empty);

#endif // COLLECTION_QUEUE_H
