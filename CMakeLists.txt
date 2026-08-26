cmake_minimum_required(VERSION 3.21)

project(collection
  VERSION      0.1.0
  DESCRIPTION  "Generic data structures and algorithms for C"
  HOMEPAGE_URL "https://github.com/derialdavi/collection"
  LANGUAGES    C)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# ----------------------------------------------------------------------------
# Options
# ----------------------------------------------------------------------------

option(COLLECTION_BUILD_SHARED "Build the shared library" ON)
option(COLLECTION_BUILD_STATIC "Build the static library" ON)
option(COLLECTION_WERROR       "Treat compiler warnings as errors" OFF)

if(NOT COLLECTION_BUILD_SHARED AND NOT COLLECTION_BUILD_STATIC)
  message(FATAL_ERROR
    "Both COLLECTION_BUILD_SHARED and COLLECTION_BUILD_STATIC are OFF; there is nothing to build.")
endif()

# Single-config generators leave CMAKE_BUILD_TYPE empty, which means no
# optimisation flags at all. Multi-config generators pick the type at build time.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    Debug Release RelWithDebInfo MinSizeRel)
endif()

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# ----------------------------------------------------------------------------
# Sources
#
# Globbed on purpose: modules are created with `ceedling module:create[name]`,
# which drops a .c/.h pair into src/. CONFIGURE_DEPENDS makes the build pick
# them up without anyone having to edit this file.
# ----------------------------------------------------------------------------

file(GLOB COLLECTION_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
file(GLOB COLLECTION_HEADERS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
list(SORT COLLECTION_SOURCES)
list(SORT COLLECTION_HEADERS)

if(NOT COLLECTION_SOURCES)
  message(FATAL_ERROR
    "No sources found in src/.\n"
    "Create the first module with:  ceedling module:create[name]")
endif()

# ----------------------------------------------------------------------------
# Generated headers
#
# The build tree mirrors the install tree: headers are copied to
# <build>/include/collection/ and the umbrella lands at <build>/include/
# so that <collection.h> and <collection/queue.h> resolve identically whether
# the library is consumed from a build directory or from an install prefix.
#
# configure_file() registers its input as a configure-time dependency, so
# editing a header re-runs CMake and refreshes the copy.
# ----------------------------------------------------------------------------

set(COLLECTION_GENERATED_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
set(COLLECTION_UMBRELLA_INCLUDES "")

foreach(header IN LISTS COLLECTION_HEADERS)
  get_filename_component(header_name "${header}" NAME)
  configure_file("${header}"
    "${COLLECTION_GENERATED_INCLUDE_DIR}/collection/${header_name}" COPYONLY)
  string(APPEND COLLECTION_UMBRELLA_INCLUDES "#include <collection/${header_name}>\n")
endforeach()

configure_file(cmake/collection.h.in
  "${COLLECTION_GENERATED_INCLUDE_DIR}/collection.h" @ONLY)

# ----------------------------------------------------------------------------
# Targets
#
# The static and shared libraries are compiled separately rather than sharing an
# OBJECT library: the static build needs COLLECTION_STATIC defined and the
# shared build must not have it, so the object files are genuinely different.
# ----------------------------------------------------------------------------

function(collection_configure_target target)
  target_sources(${target} PRIVATE ${COLLECTION_SOURCES})

  set_target_properties(${target} PROPERTIES
    C_STANDARD                23
    C_STANDARD_REQUIRED       ON
    C_EXTENSIONS              OFF
    C_VISIBILITY_PRESET       hidden
    VISIBILITY_INLINES_HIDDEN ON
    POSITION_INDEPENDENT_CODE ON
    OUTPUT_NAME               collection)

  # Selects __declspec(dllexport) / visibility("default") in collection_api.h.
  target_compile_definitions(${target} PRIVATE COLLECTION_BUILDING)

  target_include_directories(${target}
    PUBLIC
      $<BUILD_INTERFACE:${COLLECTION_GENERATED_INCLUDE_DIR}>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
      "${CMAKE_CURRENT_SOURCE_DIR}/src")

  target_link_libraries(${target} PUBLIC Threads::Threads)

  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 $<$<BOOL:${COLLECTION_WERROR}>:/WX>)
  else()
    # Kept in step with the :flags: block in project.yml, so a module that
    # builds clean under Ceedling also builds clean here.
    target_compile_options(${target} PRIVATE
      -Wall -Wextra -Wpedantic -Wshadow -Wstrict-prototypes
      $<$<BOOL:${COLLECTION_WERROR}>:-Werror>)
  endif()
endfunction()

set(COLLECTION_INSTALL_TARGETS "")

if(COLLECTION_BUILD_SHARED)
  add_library(collection_shared SHARED)
  collection_configure_target(collection_shared)
  set_target_properties(collection_shared PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    # macOS only, and ignored elsewhere. CMake would otherwise stamp the
    # library's own name as @rpath/libcollection.0.dylib, which is a
    # placeholder every consumer has to resolve by passing -Wl,-rpath. A plain
    # `cc main.c -lcollection` passes no such thing, so the program would link
    # and then die at startup with "no LC_RPATH's found". Recording the
    # installed location instead makes that command work the way it already
    # does on Linux, where the SONAME sends the loader to the same place.
    #
    # The cost is relocation: an installed dylib that is later moved is no
    # longer found. Installing to a different prefix is fine, since this is
    # the prefix's own libdir and DESTDIR staging does not change it.
    INSTALL_NAME_DIR "${CMAKE_INSTALL_FULL_LIBDIR}")
  list(APPEND COLLECTION_INSTALL_TARGETS collection_shared)
endif()

if(COLLECTION_BUILD_STATIC)
  add_library(collection_static STATIC)
  collection_configure_target(collection_static)
  # PUBLIC so the definition reaches consumers through the exported target and
  # collection_api.h drops the import decoration.
  target_compile_definitions(collection_static PUBLIC COLLECTION_STATIC)
  if(WIN32)
    # Otherwise this archive and the DLL's import library are both collection.lib.
    set_target_properties(collection_static PROPERTIES OUTPUT_NAME collection_static)
  endif()
  list(APPEND COLLECTION_INSTALL_TARGETS collection_static)
endif()

# Canonical name for consumers, preferring the shared library.
if(COLLECTION_BUILD_SHARED)
  add_library(collection::collection ALIAS collection_shared)
else()
  add_library(collection::collection ALIAS collection_static)
endif()

# ----------------------------------------------------------------------------
# Install
# ----------------------------------------------------------------------------

install(TARGETS ${COLLECTION_INSTALL_TARGETS}
  EXPORT   collectionTargets
  RUNTIME  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  LIBRARY  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE  DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

install(FILES ${COLLECTION_HEADERS}
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/collection")

install(FILES "${COLLECTION_GENERATED_INCLUDE_DIR}/collection.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

set(COLLECTION_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/collection")

install(EXPORT collectionTargets
  FILE        collectionTargets.cmake
  NAMESPACE   collection::
  DESTINATION "${COLLECTION_CMAKE_INSTALL_DIR}")

configure_package_config_file(cmake/collectionConfig.cmake.in
  "${CMAKE_CURRENT_BINARY_DIR}/collectionConfig.cmake"
  INSTALL_DESTINATION "${COLLECTION_CMAKE_INSTALL_DIR}")

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/collectionConfigVersion.cmake"
  VERSION       ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/collectionConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/collectionConfigVersion.cmake"
  DESTINATION "${COLLECTION_CMAKE_INSTALL_DIR}")

# pkg-config, for consumers that do not use CMake.
#
# The prefix is derived from ${pcfiledir} rather than baked in, so the .pc stays
# correct when `cmake --install --prefix` overrides the configure-time prefix,
# and when the install tree is later relocated or packaged. This mirrors what
# configure_package_config_file() already does for the CMake package files.
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
  set(PKG_CONFIG_PREFIX "${CMAKE_INSTALL_PREFIX}")
else()
  file(RELATIVE_PATH PKG_CONFIG_PREFIX_RELATIVE
    "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    "${CMAKE_INSTALL_PREFIX}")
  string(REGEX REPLACE "/+$" "" PKG_CONFIG_PREFIX_RELATIVE "${PKG_CONFIG_PREFIX_RELATIVE}")
  set(PKG_CONFIG_PREFIX "\${pcfiledir}/${PKG_CONFIG_PREFIX_RELATIVE}")
endif()

# GNUInstallDirs may hand back an absolute or a relative path depending on the
# platform and prefix.
if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
  set(PKG_CONFIG_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
else()
  set(PKG_CONFIG_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()
if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
  set(PKG_CONFIG_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}")
else()
  set(PKG_CONFIG_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

set(PKG_CONFIG_LIBS_PRIVATE "")
if(CMAKE_THREAD_LIBS_INIT)
  set(PKG_CONFIG_LIBS_PRIVATE "${CMAKE_THREAD_LIBS_INIT}")
endif()

configure_file(cmake/collection.pc.in
  "${CMAKE_CURRENT_BINARY_DIR}/collection.pc" @ONLY)

install(FILES "${CMAKE_CURRENT_BINARY_DIR}/collection.pc"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

# ----------------------------------------------------------------------------
# Uninstall
# ----------------------------------------------------------------------------

if(NOT TARGET uninstall)
  configure_file(cmake/uninstall.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake" @ONLY)
  add_custom_target(uninstall
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/uninstall.cmake"
    COMMENT "Removing every file listed in install_manifest.txt")
endif()

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------

list(LENGTH COLLECTION_SOURCES COLLECTION_SOURCE_COUNT)
message(STATUS "collection ${PROJECT_VERSION}: ${COLLECTION_SOURCE_COUNT} source(s), "
               "shared=${COLLECTION_BUILD_SHARED} static=${COLLECTION_BUILD_STATIC}, "
               "install prefix ${CMAKE_INSTALL_PREFIX}")
