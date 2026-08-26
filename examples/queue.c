/*
 * queue - example.
 *
 * Every function in queue.h, called once, in the order you would use it.
 *
 *     cc -std=c2x examples/queue.c -lcollection -o queue
 */

#include <collection/queue.h>

#include <stdio.h>

int main(void)
{
    coll_queue q;
    int        rc;

    /* init: 0 asks for an empty queue. The last two arguments prefill it, and
       are ignored here */
    rc = coll_queue_init(&q, 0, NULL, 0);
    if (rc != COLLECTION_OK) { printf("init: %d\n", rc); return 1; }

    /* is_empty */
    bool empty = false;
    rc = coll_queue_is_empty(&q, &empty);
    if (rc != COLLECTION_OK) { printf("is_empty: %d\n", rc); return 1; }
    printf("empty to start with? %s\n", empty ? "yes" : "no");

    /* enqueue: joins the back of the queue. The value is copied in, so it
       does not have to outlive this call */
    int value = 10;
    rc = coll_queue_enqueue(&q, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("enqueue: %d\n", rc); return 1; }

    value = 20;
    rc = coll_queue_enqueue(&q, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("enqueue: %d\n", rc); return 1; }

    value = 30;
    rc = coll_queue_enqueue(&q, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("enqueue: %d\n", rc); return 1; }

    /* get_size */
    size_t size = 0;
    rc = coll_queue_get_size(&q, &size);
    if (rc != COLLECTION_OK) { printf("get_size: %d\n", rc); return 1; }
    printf("%zu elements waiting\n", size);

    /* peek: reads the element at the front without taking it */
    int out = 0;
    rc = coll_queue_peek(&q, &out, sizeof(out));
    if (rc != COLLECTION_OK) { printf("peek: %d\n", rc); return 1; }
    printf("front is %d\n", out);

    /* peek_size: how big the front element is, when you do not already know.
       A queue may hold elements of different sizes */
    size_t elem_size = 0;
    rc = coll_queue_peek_size(&q, &elem_size);
    if (rc != COLLECTION_OK) { printf("peek_size: %d\n", rc); return 1; }
    printf("front element is %zu bytes\n", elem_size);

    /* dequeue: takes the element at the front, first in first out. The size
       must match the one it was stored with, so nothing gets truncated */
    rc = coll_queue_dequeue(&q, &out, sizeof(out));
    if (rc != COLLECTION_OK) { printf("dequeue: %d\n", rc); return 1; }
    printf("dequeued %d\n", out);

    /* the wrong size is COLLECTION_EINVAL, and the element stays put, so you
       can ask coll_queue_peek_size() and try again */
    long long wide = 0;
    rc = coll_queue_dequeue(&q, &wide, sizeof(wide));
    printf("dequeue at the wrong size: %s\n",
           rc == COLLECTION_EINVAL ? "COLLECTION_EINVAL" : "??");

    /* drain the rest, in the order they went in */
    while ((rc = coll_queue_dequeue(&q, &out, sizeof(out))) == COLLECTION_OK)
        printf("dequeued %d\n", out);

    /* an empty queue is COLLECTION_ENOTFOUND, not a crash */
    if (rc != COLLECTION_ENOTFOUND) { printf("dequeue: %d\n", rc); return 1; }
    printf("dequeue when empty: COLLECTION_ENOTFOUND\n");

    /* peek says the same on an empty queue */
    rc = coll_queue_peek(&q, &out, sizeof(out));
    printf("peek when empty: %s\n",
           rc == COLLECTION_ENOTFOUND ? "COLLECTION_ENOTFOUND" : "??");

    /* destroy: frees every element. The coll_queue itself is yours, and it is
       empty and reusable afterwards */
    rc = coll_queue_destroy(&q);
    if (rc != COLLECTION_OK) { printf("destroy: %d\n", rc); return 1; }

    /* init again, this time prefilled with 4 copies of one value */
    value = 7;
    rc = coll_queue_init(&q, 4, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("init: %d\n", rc); return 1; }

    rc = coll_queue_get_size(&q, &size);
    if (rc != COLLECTION_OK) { printf("get_size: %d\n", rc); return 1; }
    printf("prefilled with %zu copies of %d\n", size, value);

    rc = coll_queue_destroy(&q);
    if (rc != COLLECTION_OK) { printf("destroy: %d\n", rc); return 1; }

    return 0;
}
