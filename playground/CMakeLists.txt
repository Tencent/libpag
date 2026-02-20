cmake_minimum_required(VERSION 3.13)
project(PAGXPlayground)

# Include vendor tools from libpag root
include(${CMAKE_CURRENT_SOURCE_DIR}/../third_party/vendor_tools/vendor.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()

# Set TGFX_DIR for libpag structure (tgfx is in third_party/tgfx)
if (NOT TGFX_DIR)
    set(TGFX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tgfx)
endif ()
get_filename_component(TGFX_DIR "${TGFX_DIR}" REALPATH)

# Add tgfx as dependency
if (NOT TARGET tgfx)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(TGFX_BUILD_LAYERS ON CACHE BOOL "" FORCE)
    add_subdirectory(${TGFX_DIR} ${CMAKE_CURRENT_BINARY_DIR}/tgfx)
endif ()

# Build pagx static library
file(GLOB PAGX_CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/pagx/*.cpp)
file(GLOB_RECURSE PAGX_UTILS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/pagx/utils/*.cpp)
file(GLOB_RECURSE PAGX_XML_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/pagx/xml/*.cpp)
file(GLOB_RECURSE PAGX_SVG_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/pagx/svg/*.cpp)
if (NOT TARGET pagx)
    add_library(pagx STATIC ${PAGX_CORE_SOURCES} ${PAGX_UTILS_SOURCES} ${PAGX_XML_SOURCES} ${PAGX_SVG_SOURCES})
    target_include_directories(pagx PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    target_include_directories(pagx PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/pagx
            ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/expat/expat/lib)
    if (WIN32)
        target_compile_definitions(pagx PRIVATE XML_STATIC)
    endif ()
    add_vendor_target(pagx-vendor STATIC_VENDORS expat CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
    find_vendor_libraries(pagx-vendor STATIC PAGX_VENDOR_STATIC_LIBRARIES)
    add_dependencies(pagx pagx-vendor)
    merge_libraries_into(pagx ${PAGX_VENDOR_STATIC_LIBRARIES})
endif ()

# Build renderer sources as a static library for playground use
file(GLOB_RECURSE RENDERER_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer/*.cpp)
if (NOT TARGET pagx-renderer)
    add_library(pagx-renderer STATIC ${RENDERER_SOURCES})
    target_include_directories(pagx-renderer PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/renderer)
    target_include_directories(pagx-renderer PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../src/pagx)
    target_link_libraries(pagx-renderer PUBLIC pagx tgfx)
endif ()

# For Emscripten builds, pagx needs the same compile options as tgfx
if (DEFINED EMSCRIPTEN)
    target_compile_options(pagx PRIVATE -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0)
    target_compile_options(pagx-renderer PRIVATE -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0)
endif ()

file(GLOB_RECURSE PLAYGROUND_FILES src/*.cpp)

if (DEFINED EMSCRIPTEN)
    add_executable(pagx-playground ${PLAYGROUND_FILES})
    list(APPEND PLAYGROUND_COMPILE_OPTIONS -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0)
    list(APPEND PLAYGROUND_LINK_OPTIONS --no-entry -lembind -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 -sEXPORT_NAME='PAGXWasm' -sWASM=1
            -sMAX_WEBGL_VERSION=2 -sEXPORTED_RUNTIME_METHODS=['GL','HEAPU8'] -sMODULARIZE=1
            -sENVIRONMENT=web,worker -sEXPORT_ES6=1)
    set(unsupported_emsdk_versions "4.0.11")
    foreach (unsupported_version IN LISTS unsupported_emsdk_versions)
        if (${EMSCRIPTEN_VERSION} VERSION_EQUAL ${unsupported_version})
            message(FATAL_ERROR "Emscripten version ${EMSCRIPTEN_VERSION} is not supported.")
        endif ()
    endforeach ()
    if (EMSCRIPTEN_PTHREADS)
        list(APPEND PLAYGROUND_LINK_OPTIONS -sUSE_PTHREADS=1 -sINITIAL_MEMORY=32MB -sALLOW_MEMORY_GROWTH=1
                -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency
                -sEXIT_RUNTIME=0 -sINVOKE_RUN=0 -sMALLOC=dlmalloc)
        list(APPEND PLAYGROUND_COMPILE_OPTIONS -fPIC -pthread)
    else ()
        list(APPEND PLAYGROUND_LINK_OPTIONS -sALLOW_MEMORY_GROWTH=1)
    endif ()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND PLAYGROUND_COMPILE_OPTIONS -O0 -g3)
        list(APPEND PLAYGROUND_LINK_OPTIONS -O0 -g3 -sSAFE_HEAP=1 -Wno-limited-postlink-optimizations)
    else ()
        list(APPEND PLAYGROUND_COMPILE_OPTIONS -Oz)
        list(APPEND PLAYGROUND_LINK_OPTIONS -Oz)
    endif ()
else ()
    add_library(pagx-playground SHARED ${PLAYGROUND_FILES})
endif ()

target_compile_options(pagx-playground PUBLIC ${PLAYGROUND_COMPILE_OPTIONS})
target_link_options(pagx-playground PUBLIC ${PLAYGROUND_LINK_OPTIONS})
target_link_libraries(pagx-playground pagx-renderer)
