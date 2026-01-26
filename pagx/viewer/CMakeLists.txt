cmake_minimum_required(VERSION 3.13)
project(PAGXViewer)
# Disable error for the web platform.
SET_PROPERTY(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS true)

# Include vendor tools from libpag root
include(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/vendor_tools/vendor.cmake)

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

# Add tgfx as dependency (supports both cached build and source build)
if (NOT TARGET tgfx)
    list(APPEND PAGX_VIEWER_INCLUDES ${TGFX_DIR}/include)
    set(TGFX_CACHE_DIR ${TGFX_DIR}/out/cache)
    if (EXISTS ${TGFX_CACHE_DIR})
        # Use cached tgfx build
        message("TGFX_CACHE_DIR: ${TGFX_CACHE_DIR}")
        list(APPEND TGFX_OPTIONS "-DTGFX_BUILD_SVG=ON")
        list(APPEND TGFX_OPTIONS "-DTGFX_BUILD_LAYERS=ON")
        if (CMAKE_BUILD_TYPE MATCHES "Debug")
            list(APPEND TGFX_OPTIONS "-d")
            string(TOLOWER ${CMAKE_BUILD_TYPE} TGFX_BUILD_TYPE)
        endif ()
        set(TGFX_OUTPUT_DIR ${TGFX_CACHE_DIR}/${TGFX_BUILD_TYPE}/${PLATFORM})
        set(TGFX_OUTPUT_LIB ${TGFX_OUTPUT_DIR}/${ARCH}/tgfx${CMAKE_STATIC_LIBRARY_SUFFIX})
        set(TGFX_OUTPUT_MD5 ${TGFX_OUTPUT_DIR}/.tgfx.${ARCH}.md5)
        add_custom_target(tgfx
                COMMAND node build_tgfx ${TGFX_OPTIONS} -p ${PLATFORM} -a ${ARCH} -o ${TGFX_OUTPUT_DIR}
                WORKING_DIRECTORY ${TGFX_DIR}
                BYPRODUCTS ${TGFX_OUTPUT_LIB} ${TGFX_OUTPUT_MD5}
                VERBATIM USES_TERMINAL)
        list(APPEND PAGX_VIEWER_STATIC_LIBS ${TGFX_OUTPUT_LIB})
        list(APPEND PAGX_VIEWER_DEPEND_TARGETS tgfx)
    else ()
        # Fallback to source build
        message("TGFX_DIR: ${TGFX_DIR}")
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(TGFX_BUILD_SVG ON CACHE BOOL "" FORCE)
        set(TGFX_BUILD_LAYERS ON CACHE BOOL "" FORCE)
        add_subdirectory(${TGFX_DIR} ${CMAKE_CURRENT_BINARY_DIR}/tgfx)
        list(APPEND PAGX_VIEWER_STATIC_LIBS $<TARGET_FILE:tgfx>)
        list(APPEND PAGX_VIEWER_DEPEND_TARGETS tgfx)
    endif ()
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

if (PAGX_VIEWER_DEPEND_TARGETS)
    add_dependencies(pagx-viewer ${PAGX_VIEWER_DEPEND_TARGETS})
endif ()
target_include_directories(pagx-viewer PRIVATE ${PAGX_VIEWER_INCLUDES})
target_compile_options(pagx-viewer PUBLIC ${PAGX_VIEWER_COMPILE_OPTIONS})
target_link_options(pagx-viewer PUBLIC ${PAGX_VIEWER_LINK_OPTIONS})
target_link_libraries(pagx-viewer pagx ${PAGX_VIEWER_STATIC_LIBS})
