cmake_minimum_required(VERSION 3.13)
project(PAGXViewer)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()

# Set TGFX_DIR for libpag structure (tgfx is in third_party/tgfx)
if (NOT TGFX_DIR)
    set(TGFX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/tgfx)
endif ()
get_filename_component(TGFX_DIR "${TGFX_DIR}" REALPATH)

# Add tgfx and pagx as dependencies
if (NOT TARGET tgfx)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
    set(TGFX_BUILD_SVG ON CACHE BOOL "" FORCE)
    set(TGFX_BUILD_LAYERS ON CACHE BOOL "" FORCE)
    add_subdirectory(${TGFX_DIR} ${CMAKE_CURRENT_BINARY_DIR}/tgfx)
endif ()

if (NOT TARGET pagx)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/pagx)
endif ()

# For Emscripten builds, pagx needs the same compile options as tgfx
if (DEFINED EMSCRIPTEN)
    target_compile_options(pagx PRIVATE -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0)
endif ()

file(GLOB_RECURSE PAGX_VIEWER_FILES src/*.cpp)

if (DEFINED EMSCRIPTEN)
    add_executable(pagx-viewer ${PAGX_VIEWER_FILES})
    list(APPEND PAGX_VIEWER_COMPILE_OPTIONS -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0)
    list(APPEND PAGX_VIEWER_LINK_OPTIONS --no-entry -lembind -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 -sEXPORT_NAME='PAGXWasm' -sWASM=1
            -sMAX_WEBGL_VERSION=2 -sEXPORTED_RUNTIME_METHODS=['GL','HEAPU8'] -sMODULARIZE=1
            -sENVIRONMENT=web,worker -sEXPORT_ES6=1)
    set(unsupported_emsdk_versions "4.0.11")
    foreach (unsupported_version IN LISTS unsupported_emsdk_versions)
        if (${EMSCRIPTEN_VERSION} VERSION_EQUAL ${unsupported_version})
            message(FATAL_ERROR "Emscripten version ${EMSCRIPTEN_VERSION} is not supported.")
        endif ()
    endforeach ()
    if (EMSCRIPTEN_PTHREADS)
        list(APPEND PAGX_VIEWER_LINK_OPTIONS -sUSE_PTHREADS=1 -sINITIAL_MEMORY=32MB -sALLOW_MEMORY_GROWTH=1
                -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency
                -sEXIT_RUNTIME=0 -sINVOKE_RUN=0 -sMALLOC=mimalloc)
        list(APPEND PAGX_VIEWER_COMPILE_OPTIONS -fPIC -pthread)
    else ()
        list(APPEND PAGX_VIEWER_LINK_OPTIONS -sALLOW_MEMORY_GROWTH=1)
    endif ()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND PAGX_VIEWER_COMPILE_OPTIONS -O0 -g3)
        list(APPEND PAGX_VIEWER_LINK_OPTIONS -O0 -g3 -sSAFE_HEAP=1 -Wno-limited-postlink-optimizations)
    else ()
        list(APPEND PAGX_VIEWER_COMPILE_OPTIONS -Oz)
        list(APPEND PAGX_VIEWER_LINK_OPTIONS -Oz)
    endif ()
else ()
    add_library(pagx-viewer SHARED ${PAGX_VIEWER_FILES})
endif ()

target_compile_options(pagx-viewer PUBLIC ${PAGX_VIEWER_COMPILE_OPTIONS})
target_link_options(pagx-viewer PUBLIC ${PAGX_VIEWER_LINK_OPTIONS})
target_link_libraries(pagx-viewer pagx)
