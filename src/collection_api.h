#ifndef COLLECTION_API_H
#define COLLECTION_API_H

/*
 * COLLECTION_API marks the public surface of the library.
 *
 * Windows exports nothing from a DLL unless a symbol is tagged, while ELF and
 * Mach-O export every non-static symbol by default. The library is compiled
 * with hidden visibility, so on every platform the exported set is exactly the
 * set of declarations carrying this macro.
 *
 * COLLECTION_BUILDING is defined by the build system while compiling the
 * library itself; COLLECTION_STATIC is propagated to consumers of the static
 * library, where no import/export decoration applies.
 */

#if defined(_WIN32) || defined(__CYGWIN__)
#  if defined(COLLECTION_STATIC)
#    define COLLECTION_API
#  elif defined(COLLECTION_BUILDING)
#    define COLLECTION_API __declspec(dllexport)
#  else
#    define COLLECTION_API __declspec(dllimport)
#  endif
#elif defined(COLLECTION_BUILDING) && (defined(__GNUC__) || defined(__clang__))
#  define COLLECTION_API __attribute__((visibility("default")))
#else
#  define COLLECTION_API
#endif

#endif /* COLLECTION_API_H */
