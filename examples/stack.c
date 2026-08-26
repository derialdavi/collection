/*
 * stack - example.
 *
 * Every function in stack.h, called once, in the order you would use it.
 *
 *     cc -std=c2x examples/stack.c -lcollection -o stack
 */

#include <collection/stack.h>

#include <stdio.h>

int main(void)
{
    /* the type is cstack_t, not stack_t: POSIX already uses that name in
       <signal.h>. Every function keeps the plain stack_ prefix */
    cstack_t s;
    int      rc;

    /* init: 0 asks for an empty stack. The last two arguments prefill it, and
       are ignored here */
    rc = stack_init(&s, 0, NULL, 0);
    if (rc != COLLECTION_OK) { printf("init: %d\n", rc); return 1; }

    /* is_empty */
    bool empty = false;
    rc = stack_is_empty(&s, &empty);
    if (rc != COLLECTION_OK) { printf("is_empty: %d\n", rc); return 1; }
    printf("empty to start with? %s\n", empty ? "yes" : "no");

    /* push: goes on top. The value is copied in, so it does not have to
       outlive this call */
    int value = 10;
    rc = stack_push(&s, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("push: %d\n", rc); return 1; }

    value = 20;
    rc = stack_push(&s, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("push: %d\n", rc); return 1; }

    value = 30;
    rc = stack_push(&s, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("push: %d\n", rc); return 1; }

    /* get_size */
    size_t size = 0;
    rc = stack_get_size(&s, &size);
    if (rc != COLLECTION_OK) { printf("get_size: %d\n", rc); return 1; }
    printf("%zu elements stacked up\n", size);

    /* peek: reads the element on top without taking it */
    int out = 0;
    rc = stack_peek(&s, &out, sizeof(out));
    if (rc != COLLECTION_OK) { printf("peek: %d\n", rc); return 1; }
    printf("top is %d\n", out);

    /* peek_size: how big the top element is, when you do not already know.
       A stack may hold elements of different sizes */
    size_t elem_size = 0;
    rc = stack_peek_size(&s, &elem_size);
    if (rc != COLLECTION_OK) { printf("peek_size: %d\n", rc); return 1; }
    printf("top element is %zu bytes\n", elem_size);

    /* pop: takes the element on top, last in first out. The size must match
       the one it was stored with, so nothing gets truncated */
    rc = stack_pop(&s, &out, sizeof(out));
    if (rc != COLLECTION_OK) { printf("pop: %d\n", rc); return 1; }
    printf("popped %d\n", out);

    /* the wrong size is COLLECTION_EINVAL, and the element stays put, so you
       can ask stack_peek_size() and try again */
    long long wide = 0;
    rc = stack_pop(&s, &wide, sizeof(wide));
    printf("pop at the wrong size: %s\n",
           rc == COLLECTION_EINVAL ? "COLLECTION_EINVAL" : "??");

    /* empty the rest, in the reverse of the order they went in */
    while ((rc = stack_pop(&s, &out, sizeof(out))) == COLLECTION_OK)
        printf("popped %d\n", out);

    /* an empty stack is COLLECTION_ENOTFOUND, not a crash */
    if (rc != COLLECTION_ENOTFOUND) { printf("pop: %d\n", rc); return 1; }
    printf("pop when empty: COLLECTION_ENOTFOUND\n");

    /* peek says the same on an empty stack */
    rc = stack_peek(&s, &out, sizeof(out));
    printf("peek when empty: %s\n",
           rc == COLLECTION_ENOTFOUND ? "COLLECTION_ENOTFOUND" : "??");

    /* destroy: frees every element. The cstack_t itself is yours, and it is
       empty and reusable afterwards */
    rc = stack_destroy(&s);
    if (rc != COLLECTION_OK) { printf("destroy: %d\n", rc); return 1; }

    /* init again, this time prefilled with 4 copies of one value */
    value = 7;
    rc = stack_init(&s, 4, &value, sizeof(value));
    if (rc != COLLECTION_OK) { printf("init: %d\n", rc); return 1; }

    rc = stack_get_size(&s, &size);
    if (rc != COLLECTION_OK) { printf("get_size: %d\n", rc); return 1; }
    printf("prefilled with %zu copies of %d\n", size, value);

    rc = stack_destroy(&s);
    if (rc != COLLECTION_OK) { printf("destroy: %d\n", rc); return 1; }

    return 0;
}
