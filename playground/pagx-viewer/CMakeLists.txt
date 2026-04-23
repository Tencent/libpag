cmake_minimum_required(VERSION 3.13)
project(PAGXViewer)

# Include vendor tools from libpag root (playground/pagx-viewer -> libpag root)
set(LIBPAG_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)
include(${LIBPAG_ROOT}/third_party/vendor_tools/vendor.cmake)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()

# Enable PAGX module and add the main libpag project as a subdirectory.
# PAG_USE_FREETYPE must be ON: tgfx defaults to FreeType on WEB+threads, but the main project
# forces it OFF for WEB. Playground needs FreeType for HarfBuzz text shaping.
set(PAG_BUILD_PAGX ON)
set(PAG_USE_FREETYPE ON)
set(PAG_USE_JPEG_DECODE ON)
set(PAG_USE_PNG_DECODE ON)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(${LIBPAG_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/libpag)

# PAGX Viewer library
file(GLOB_RECURSE VIEWER_FILES src/cpp/*.cpp)

if (DEFINED EMSCRIPTEN)
    add_executable(pagx-viewer ${VIEWER_FILES})
    list(APPEND VIEWER_COMPILE_OPTIONS -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0)
    list(APPEND VIEWER_LINK_OPTIONS --no-entry -lembind -fno-rtti -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0 -sEXPORT_NAME='PAGXViewerWasm' -sWASM=1
            -sMAX_WEBGL_VERSION=2 -sEXPORTED_RUNTIME_METHODS=['GL','HEAPU8'] -sMODULARIZE=1
            -sENVIRONMENT=web,worker -sEXPORT_ES6=1)
    set(unsupported_emsdk_versions "4.0.11")
    foreach (unsupported_version IN LISTS unsupported_emsdk_versions)
        if (${EMSCRIPTEN_VERSION} VERSION_EQUAL ${unsupported_version})
            message(FATAL_ERROR "Emscripten version ${EMSCRIPTEN_VERSION} is not supported.")
        endif ()
    endforeach ()
    if (EMSCRIPTEN_PTHREADS)
        list(APPEND VIEWER_LINK_OPTIONS -sUSE_PTHREADS=1 -sINITIAL_MEMORY=32MB -sALLOW_MEMORY_GROWTH=1
                -sPTHREAD_POOL_SIZE=navigator.hardwareConcurrency
                -sEXIT_RUNTIME=0 -sINVOKE_RUN=0 -sMALLOC=dlmalloc)
        list(APPEND VIEWER_COMPILE_OPTIONS -fPIC -pthread)
    else ()
        list(APPEND VIEWER_LINK_OPTIONS -sALLOW_MEMORY_GROWTH=1)
    endif ()
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        list(APPEND VIEWER_COMPILE_OPTIONS -O0 -g3)
        list(APPEND VIEWER_LINK_OPTIONS -O0 -g3 -sSAFE_HEAP=1 -Wno-limited-postlink-optimizations)
    else ()
        list(APPEND VIEWER_COMPILE_OPTIONS -Oz)
        list(APPEND VIEWER_LINK_OPTIONS -Oz)
    endif ()
else ()
    add_library(pagx-viewer SHARED ${VIEWER_FILES})
endif ()

target_compile_options(pagx-viewer PUBLIC ${VIEWER_COMPILE_OPTIONS})
target_link_options(pagx-viewer PUBLIC ${VIEWER_LINK_OPTIONS})
# Link the complete pag static library (dead code is eliminated by wasm-ld + Binaryen Meta-DCE).
# Also link tgfx to get its PUBLIC include directories (pag links tgfx via file path, not target).
target_link_libraries(pagx-viewer pag tgfx)
target_include_directories(pagx-viewer PRIVATE
        ${LIBPAG_ROOT}/src
        ${LIBPAG_ROOT}/src/renderer)
