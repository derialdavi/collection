/*
 * %1$s - usage example.
 *
 * Build it against an installed collection with:
 *   cc -std=c2x %1$s.c -lcollection -o %1$s
 */

#include <collection/%1$s.h>

#include <stdio.h>

int main(void)
{
    coll_%1$s %1$s;

    if (coll_%1$s_init(&%1$s) != COLLECTION_OK)
    {
        fprintf(stderr, "coll_%1$s_init() failed\n");
        return 1;
    }

    coll_%1$s_destroy(&%1$s);

    return 0;
}
