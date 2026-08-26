#ifndef %3$s_H
#define %3$s_H

%2$s
/**
 * @brief
 */
typedef struct coll_%1$s
{

} coll_%1$s;

/**
 * @brief initializes a %1$s
 *
 * @param %1$s pointer to the %1$s to initialize
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if %1$s is NULL
 */
COLLECTION_API int coll_%1$s_init(coll_%1$s *restrict %1$s);

/**
 * @brief frees every element of the %1$s, leaving it empty
 *
 * @param %1$s pointer to the %1$s to empty
 * @return int COLLECTION_OK on success, COLLECTION_ENULL if %1$s is NULL
 * @note the coll_%1$s belongs to the caller and is not freed.
 * Afterwards %1$s is a valid empty %1$s and can be reused
 * withoud calling coll_%1$s_init() again
 */
COLLECTION_API int coll_%1$s_destroy(coll_%1$s *restrict %1$s);

#endif // %3$s_H
