#ifndef COLLECTION_ERROR_H
#define COLLECTION_ERROR_H

/*
 * Status codes shared by every module.
 *
 * Functions return COLLECTION_OK, which is 0, on success and a negative code
 * on failure. A caller that only wants to know whether something went wrong
 * can test `rc < 0`; a caller that cares which failure it was can compare
 * against the individual codes.
 *
 * The shared set below owns -1 through -99. A module that needs to report a
 * failure none of these can express defines its own code in its own header,
 * counting down from COLLECTION_EMODULE_BASE and named E_<MODULE>_<NAME>:
 *
 *     #define E_LIST_SOMETHING (COLLECTION_EMODULE_BASE - 0)
 *     #define E_LIST_OTHER     (COLLECTION_EMODULE_BASE - 1)
 *
 * Prefer a shared code whenever one fits. Code that handles the shared set
 * keeps working against every module, while a module-specific code has to be
 * handled by name and only means something for that one container.
 */

#define COLLECTION_OK              0   /* the call succeeded */
#define COLLECTION_ENULL         (-1)  /* a required pointer argument was NULL */
#define COLLECTION_ENOMEM        (-2)  /* an allocation failed */
#define COLLECTION_EINVAL        (-3)  /* an argument held an unusable value */
#define COLLECTION_ERANGE        (-4)  /* an index was outside the valid range */
#define COLLECTION_ENOTFOUND     (-5)  /* the requested value is not present */

#define COLLECTION_EMODULE_BASE  (-100)

#endif /* COLLECTION_ERROR_H */
