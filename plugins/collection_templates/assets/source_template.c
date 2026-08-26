%2$s#include "%1$s.h"

#include <stddef.h>

int coll_%1$s_init(coll_%1$s *restrict %1$s)
{
    if (%1$s == NULL) return COLLECTION_ENULL;

    return COLLECTION_OK;
}

int coll_%1$s_destroy(coll_%1$s *restrict %1$s)
{
    if (%1$s == NULL) return COLLECTION_ENULL;

    return COLLECTION_OK;
}
