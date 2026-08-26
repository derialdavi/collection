# Collection

Collection is a C library of generic data structures and concurrency primitives.

Containers are type-erased: they store `void *` plus a size and copy the bytes you hand
them, rather than relying on macros or code generation. One library ships as both a static
archive and a shared library, and installs so that consumers write:

```c
#include <collection.h>
```

The project targets C23 and builds on Linux, macOS and Windows.

## Requirements

To build and install the library:

- A C23 compiler: GCC 13+, Clang 16+, or MSVC 19.39+ (Visual Studio 2022 17.9+)
- CMake 3.21 or newer

To contribute, you also need the test toolchain:

- Ruby 3.2 or newer
- [Ceedling](https://github.com/ThrowTheSwitch/Ceedling) 1.1+: `gem install ceedling`

## Building

```bash
cmake -B build
cmake --build build
```

This produces a shared library and a static library side by side in `build/`:

| Platform | Shared | Static |
| --- | --- | --- |
| Linux | `libcollection.so.0.1.0` with soname `libcollection.so.0` | `libcollection.a` |
| macOS | `libcollection.0.1.0.dylib` | `libcollection.a` |
| Windows (MSVC) | `collection.dll` plus import library `collection.lib` | `collection_static.lib` |
| Windows (MinGW) | `libcollection.dll` plus import library `libcollection.dll.a` | `libcollection_static.a` |

The build type defaults to `Release` on single-config generators. Override it as usual with
`-DCMAKE_BUILD_TYPE=Debug`.

### Options

| Option | Default | Effect |
| --- | --- | --- |
| `COLLECTION_BUILD_SHARED` | `ON` | Build the shared library |
| `COLLECTION_BUILD_STATIC` | `ON` | Build the static library |
| `COLLECTION_WERROR` | `OFF` | Treat compiler warnings as errors. Intended for CI |

```bash
cmake -B build -DCOLLECTION_BUILD_STATIC=OFF -DCOLLECTION_WERROR=ON
```

### Windows

With Visual Studio, which is a multi-config generator, so the configuration is chosen at
build time rather than at configure time:

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

With MSYS2 or MinGW-w64:

```bash
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

Modules that use POSIX threads compile under MinGW, which provides winpthreads, but not
under MSVC. That is a limitation of the module source, not of the build: such a module
needs a Win32 threads path before it will build with MSVC.

## Installing

```bash
cmake --build build
sudo cmake --install build
```

The default prefix is `/usr/local` on Linux and macOS and `C:/Program Files/collection` on
Windows. Install elsewhere with `--prefix`, which does not require a reconfigure:

```bash
cmake --install build --prefix ~/.local
```

The installed tree is:

```
<prefix>/include/collection.h              umbrella header, includes every module
<prefix>/include/collection/queue.h        one header per module
<prefix>/lib/libcollection.so*             shared library and version symlinks
<prefix>/lib/libcollection.a               static library
<prefix>/lib/cmake/collection/             CMake package, for find_package()
<prefix>/lib/pkgconfig/collection.pc       pkg-config metadata
```

On Linux, refresh the dynamic linker cache after installing to a system prefix:

```bash
sudo ldconfig
```

On Windows, the DLL is installed to `<prefix>/bin`; add that directory to `PATH` so
programs can find it at run time.

For packaging, `DESTDIR` is honoured:

```bash
DESTDIR=/tmp/staging cmake --install build
```

### Uninstalling

```bash
cmake --build build --target uninstall
```

This removes every file recorded in `build/install_manifest.txt` and prunes the directories
that the install created and that are now empty. It leaves anything else in the prefix
alone, so it is safe to run against `/usr/local`. Use the same build directory that
performed the install.

## Using the library

Include the umbrella header for everything, or a single module directly:

```c
#include <collection.h>          /* every module */
#include <collection/queue.h>    /* just the queue */
```

`<collection.h>` also defines `COLLECTION_VERSION_MAJOR`, `COLLECTION_VERSION_MINOR`,
`COLLECTION_VERSION_PATCH` and `COLLECTION_VERSION_STRING`.

### With CMake

```cmake
find_package(collection 0.1 REQUIRED)

add_executable(app main.c)
target_link_libraries(app PRIVATE collection::collection)
```

`collection::collection` resolves to the shared library when one is installed. To link the
static library instead, either set `collection_USE_STATIC` before `find_package`, or name
the target explicitly:

```cmake
target_link_libraries(app PRIVATE collection::collection_static)
```

If the library is installed outside the default search paths, point CMake at it with
`-DCMAKE_PREFIX_PATH=<prefix>`.

### With pkg-config

```bash
cc main.c $(pkg-config --cflags --libs collection) -o app
```

The generated `collection.pc` computes its prefix from its own location, so it stays
correct if the install tree is moved or staged.

### With raw compiler flags

```bash
cc main.c -lcollection -o app
```

For a prefix the compiler does not search by default:

```bash
cc main.c -I<prefix>/include -L<prefix>/lib -lcollection -Wl,-rpath,<prefix>/lib -o app
```

When linking the static library on Windows, define `COLLECTION_STATIC` so that the headers
do not decorate declarations with `__declspec(dllimport)`. The CMake and pkg-config paths
handle this for you.

## Repository layout

```
CMakeLists.txt        the build
cmake/                templates: umbrella header, package config, pkg-config, uninstall
src/                  one .c and one .h per module, plus collection_api.h and collection_error.h
test/                 one test_<module>.c per module
test/support/         Ceedling support files
mixins/               opt-in Ceedling config overlays, applied with --mixin
project.yml           Ceedling configuration
.github/workflows/    CI
build/                CMake output; Ceedling nests its own output in build/ceedling
```

Both build systems share `build/`, so a single `rm -rf build` cleans everything and one
`.gitignore` entry covers both. `collection.h` does not exist in the repository: CMake
generates it from the contents of `src/` on every configure.

## Contributing

Work on a branch and open a pull request. `main` is protected and rejects direct pushes.

### Creating a module

Always use Ceedling's generator rather than adding files by hand:

```bash
ceedling 'module:create[queue]'
```

That creates `src/queue.c`, `src/queue.h` and `test/test_queue.c` together, each already
carrying this project's boilerplate. `ceedling 'module:destroy[queue]'` removes the three
again. Quote the task name: most shells, `zsh` in particular, otherwise try to expand the
square brackets as a glob.

No build file needs editing. `CMakeLists.txt` globs `src/*.c` and `src/*.h` with
`CONFIGURE_DEPENDS`, so a new module is compiled, added to the umbrella header, and
installed automatically the next time you build.

A freshly generated module warns with `-Wempty-translation-unit` until you put a function
in it. That is expected, and it is also why CI runs with `COLLECTION_WERROR=ON`: an empty
module should not be committed.

### Running the tests

```bash
ceedling test:all          # every module
ceedling test:queue        # one module
```

Tests are Ceedling's job, not CMake's; there is no `test` target in the build.

To reproduce the memory checks that CI runs, apply the sanitizer mixin:

```bash
ceedling --mixin=mixins/sanitize.yml test:all
```

That rebuilds the suite under AddressSanitizer, LeakSanitizer and
UndefinedBehaviorSanitizer. It is kept out of `project.yml` so ordinary runs stay fast.
LeakSanitizer only exists on Linux, so a local run on macOS checks memory errors and
undefined behaviour but not leaks.

### Continuous integration

[`.github/workflows/ci.yml`](.github/workflows/ci.yml) runs on every push and on every
pull request targeting `main`:

| Job | What it does |
| --- | --- |
| `tests` | `ceedling test:all` on Linux |
| `memory` | the same suite under ASan, LSan and UBSan |
| `build (ubuntu/macos/windows)` | configures, builds and installs with `COLLECTION_WERROR=ON` |

`tests` and `memory` are required status checks: a pull request cannot be merged into
`main` until both are green.

### Naming conventions

| Thing | Rule | Example |
| --- | --- | --- |
| Files | Lowercase, one `.c` and `.h` pair per module | `src/hashtable.c`, `src/hashtable.h` |
| Test files | `test_<module>.c` | `test/test_hashtable.c` |
| Test cases | `test_<module>_should_<Behaviour>` | `test_queue_should_ReturnNullWhenEmpty` |
| Include guard | `COLLECTION_<MODULE>_H`, never with leading underscores, which are reserved | `COLLECTION_QUEUE_H` |
| Public functions | `coll_<module>_<verb>` | `coll_queue_enqueue`, `coll_hashtable_put` |
| Public types | `coll_<module>`, auxiliary types `coll_<module>_<thing>`, never a `_t` suffix | `coll_queue`, `coll_hashtable_pair` |
| Struct tags | The same name as the typedef | `struct coll_queue`, `struct coll_queue_node` |
| Macros and constants | `COLLECTION_<MODULE>_<NAME>` | `COLLECTION_HASHTABLE_INITIAL_BUCKETS` |
| Shared status codes | `COLLECTION_E<NAME>`, defined once in `collection_error.h` | `COLLECTION_ENOMEM` |
| Module status codes | `E_<MODULE>_<NAME>`, only when no shared code fits | `E_LIST_SOMETHING` |
| Internal helpers | `static`, in the `.c` file, no naming rule | `static struct coll_queue_node *node_create(void)` |

Every name the library exports carries the `coll_` prefix. A shared library hands its
symbols to every program that links it, and the plain names are ones a program is entitled
to use itself: `list_init`, `queue_peek` and `hashtable_put` are what anyone writing their
own container reaches for first. The prefix is what keeps a duplicate-symbol error, or a
silently wrong definition, from ever being the caller's problem. Macros keep the longer
`COLLECTION_` prefix instead: they are resolved before compilation and never reach the
linker.

Types stop at the prefixed name and take no `_t` suffix. POSIX reserves `_t` for the
implementation, and every header the standard is free to add may claim a name in it, so the
suffix buys nothing here and risks a collision the library cannot see coming. Since the
typedef no longer needs a name distinct from its struct tag, the two are spelled the same:
`struct coll_queue` and `coll_queue` name one type.

Prefer opaque types: define the struct in the `.c` file and expose only a typedef to an
incomplete type in the header, so the layout is not part of the ABI.

Headers must be self-contained. Include what you use, including `<stdbool.h>`, `<stddef.h>`
and `<stdint.h>`; do not rely on another header having pulled them in first.

### Error handling and ownership

Functions return an `int` status code, never a pointer and never a bare success flag.
`COLLECTION_OK` is `0` and every failure is negative, so a caller can test `rc < 0` for "any
error" and compare against a specific code when it needs to distinguish:

```c
coll_list l;

int rc = coll_list_init(&l, 0, NULL, 0);
if (rc < 0) return rc;

int value = 7;
if (coll_list_append(&l, &value, sizeof(value)) == COLLECTION_ENOMEM) { /* ... */ }

coll_list_destroy(&l);
```

The shared codes live in [src/collection_error.h](src/collection_error.h) and cover the
failures every container has in common:

| Code | Meaning |
| --- | --- |
| `COLLECTION_OK` | The call succeeded |
| `COLLECTION_ENULL` | A required pointer argument was NULL |
| `COLLECTION_ENOMEM` | An allocation failed |
| `COLLECTION_EINVAL` | An argument held an unusable value |
| `COLLECTION_ERANGE` | An index was outside the valid range |
| `COLLECTION_ENOTFOUND` | The requested value is not present |

Reach for a shared code whenever one fits: code written against them keeps working across
every module. Only when a module has a failure none of them can express does it define its
own, counting down from `COLLECTION_EMODULE_BASE` in its own header. The shared set owns
`-1` to `-99`, so the two ranges cannot collide.

Containers do not allocate their own handle. The caller owns the struct, so it can live on
the stack or inside another object, and the module provides an `init`/`destroy` pair rather
than a `create`/`free` one:

```c
int  coll_list_init(coll_list *l, ...);  /* initializes storage the caller supplies */
int  coll_list_destroy(coll_list *l);    /* releases the contents, not the handle */
```

`destroy` leaves the container valid and empty, so it can be reused without re-initializing,
and calling it twice is safe. Anything a container stores is still byte-copied onto the heap
and owned by the container.

### Exported symbols

Every declaration that is part of the public API must be tagged `COLLECTION_API`, which is
defined in `src/collection_api.h` and already included in generated headers:

```c
COLLECTION_API coll_queue *coll_queue_create(void);
COLLECTION_API void        coll_queue_destroy(coll_queue *queue);
```

This is not decoration. The two platform families default in opposite directions: a Windows
DLL exports nothing unless a symbol is marked `__declspec(dllexport)`, while ELF and Mach-O
export every non-static symbol. The library is compiled with `-fvisibility=hidden`, so on
every platform the exported set is exactly the set of `COLLECTION_API` declarations. An
untagged function will not be callable from a Windows DLL, and an internal helper that is
not `static` would otherwise leak into every program that links the library, where it can
collide with a symbol of the same name.

You can check what a build actually exports:

```bash
nm -gU build/libcollection.dylib      # macOS
nm -D --defined-only build/libcollection.so   # Linux
```
