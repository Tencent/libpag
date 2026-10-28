cmake_minimum_required(VERSION 3.13)
project(PAGWebTest)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_emscripten.cmake)

set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()

option(PAG_USE_WEBGPU "Build the test binary against the WebGPU backend" OFF)
option(PAG_UPDATE_BASELINE "Record MD5s into the local cache instead of comparing" OFF)
option(PAG_SKIP_GENERATE_BASELINE_IMAGES "Do not regenerate _base.webp reference images" OFF)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

# The root CMakeLists must not create native test targets that cannot compile for Emscripten.
set(PAG_BUILD_TESTS OFF)
# Mirror the native test configuration: the pagx suites need the extended formats.
set(PAG_BUILD_PAGX ON)
set(PAG_BUILD_SVG ON)
set(PAG_BUILD_HTML ON)
set(PAG_BUILD_PPT ON)
set(PAG_BUILD_CLI OFF)
# Web platform only enables WEBP decode by default. Enable encode for baseline image generation.
set(TGFX_USE_WEBP_ENCODE ON)
set(TGFX_USE_JPEG_ENCODE ON)

# Include vendor.cmake before add_subdirectory so that vendor variables (VENDOR_TOOLS_DIR,
# PLATFORM, ARCH, etc.) are set in this scope. The include_guard(GLOBAL) in vendor.cmake
# prevents it from running again inside the root CMakeLists.
include(${CMAKE_CURRENT_SOURCE_DIR}/../../third_party/vendor_tools/vendor.cmake)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. ./libpag)

set(ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Collect test sources (reuse the existing native tests).
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${ROOT}/test/src/*.*)

# Objective-C platform tests never compile under Emscripten.
list(FILTER TEST_SOURCES EXCLUDE REGEX "\\.mm$")
# The web build uses WebTestRunner.cpp as its entry point.
list(FILTER TEST_SOURCES EXCLUDE REGEX "TestMain\\.cpp$")
# Phase-1 exclusions (see README.md "Excluded on the web"):
list(FILTER TEST_SOURCES EXCLUDE REGEX "MultiThreadCase\\.cpp$")            # pthreads
list(FILTER TEST_SOURCES EXCLUDE REGEX "PAGANRRegressionTest\\.cpp$")       # pthreads + ffavc
list(FILTER TEST_SOURCES EXCLUDE REGEX "PAGDiskCacheTest\\.cpp$")           # pthreads + MEMFS disk cache
list(FILTER TEST_SOURCES EXCLUDE REGEX "PAGVideoSequenceTest\\.cpp$")       # needs wasm ffavc
list(FILTER TEST_SOURCES EXCLUDE REGEX "VideoSequenceFallbackTest\\.cpp$")  # needs wasm ffavc
list(FILTER TEST_SOURCES EXCLUDE REGEX "PAGXSVGTest\\.cpp$")                # 492MB SVG corpus
list(FILTER TEST_SOURCES EXCLUDE REGEX "PAGXCliTest\\.cpp$")                # CLI sources excluded

# Exclude hidden files (e.g., .DS_Store).
foreach (FILE_PATH ${TEST_SOURCES})
    get_filename_component(FILE_NAME "${FILE_PATH}" NAME_WE)
    if (NOT FILE_NAME)
        list(APPEND TEST_EXCLUDE_FILES ${FILE_PATH})
    endif ()
endforeach ()
if (TEST_EXCLUDE_FILES)
    list(REMOVE_ITEM TEST_SOURCES ${TEST_EXCLUDE_FILES})
endif ()

# Web test adapter sources.
file(GLOB WEB_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
list(APPEND TEST_SOURCES ${WEB_TEST_SOURCES})

# Test-only archive decoders, mirroring the native test build so that archive-producing suites
# validate the central directory, entry data, and CRCs by actually extracting.
set(MINIZIP_DIR ${TGFX_DIR}/third_party/zlib/contrib/minizip)
set(WOFF2_DIR ${ROOT}/third_party/woff2)
set(BROTLI_DIR ${WOFF2_DIR}/brotli)
file(GLOB BROTLI_DEC_SOURCES ${BROTLI_DIR}/c/dec/*.c)
list(APPEND TEST_SOURCES ${MINIZIP_DIR}/unzip.c ${WOFF2_DIR}/src/woff2_dec.cc ${BROTLI_DEC_SOURCES})
set_source_files_properties(${WOFF2_DIR}/src/woff2_dec.cc ${BROTLI_DEC_SOURCES}
        PROPERTIES COMPILE_OPTIONS "-w")

add_executable(pag-test ${TEST_SOURCES})

# Build Google Test.
add_vendor_target(test-vendor STATIC_VENDORS googletest CONFIG_DIR ${TGFX_DIR})
find_vendor_libraries(test-vendor STATIC TEST_VENDOR_LIBS)
add_dependencies(pag-test test-vendor)

# Generate the HEAD file with the current commit hash for baseline versioning.
execute_process(COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${ROOT}
        OUTPUT_VARIABLE HEAD_COMMIT OUTPUT_STRIP_TRAILING_WHITESPACE)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/HEAD "${HEAD_COMMIT}")

# Determine the backend name for the baseline cache directory.
if (PAG_USE_WEBGPU)
    set(PAG_BACKEND_NAME "webgpu")
else ()
    set(PAG_BACKEND_NAME "webgl")
endif ()

# Preload the backend-specific baseline cache if it exists locally (from a previous run's
# cache-web.zip download), so comparisons actually happen.
set(CACHE_DIR ${ROOT}/test/baseline/.cache/${PAG_BACKEND_NAME})
set(PRELOAD_CACHE "")
if (EXISTS ${CACHE_DIR}/md5.json AND EXISTS ${CACHE_DIR}/version.json)
    set(PRELOAD_CACHE
        "SHELL:--preload-file ${CACHE_DIR}/md5.json@/test/baseline/.cache/${PAG_BACKEND_NAME}/md5.json"
        "SHELL:--preload-file ${CACHE_DIR}/version.json@/test/baseline/.cache/${PAG_BACKEND_NAME}/version.json"
    )
else ()
    message(WARNING "Baseline cache not found at ${CACHE_DIR}/. "
        "Screenshot tests will pass without actual comparison. "
        "Run './update_baseline.sh USE_${PAG_BACKEND_NAME}' from project root first.")
endif ()

# Preload test resources. The whole-dataset directories mirror the native runs; the apitest
# fixtures are linked file-by-file at the top level only, excluding the 492MB apitest/SVG
# corpus that belongs to the (web-excluded) PAGXSVGTest. resources/html is 15MB of corpus for
# the HTML eval pipeline and stays preloaded because PAGX suites reference it.
set(PRELOAD_RESOURCES "")
foreach (resource_dir IN ITEMS pagx_to_html font filter timestretch text blend layout gradient
        disk fuzz compare html)
    list(APPEND PRELOAD_RESOURCES
        "SHELL:--preload-file ${ROOT}/resources/${resource_dir}@/resources/${resource_dir}")
endforeach ()
file(GLOB APITEST_TOP_LEVEL ${ROOT}/resources/apitest/*)
foreach (fixture ${APITEST_TOP_LEVEL})
    if (NOT IS_DIRECTORY ${fixture})
        get_filename_component(fixture_name "${fixture}" NAME)
        list(APPEND PRELOAD_RESOURCES "SHELL:--preload-file ${fixture}@/resources/apitest/${fixture_name}")
    endif ()
endforeach ()

target_compile_options(pag-test PRIVATE
        -fno-rtti
        -fno-exceptions
        -fno-access-control
        -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0
)

# Asyncify enables synchronous C++ code to await JS Promises. Required for the WebGPU backend
# where buffer mapping is fully asynchronous: without it, the synchronous readPixels() used by
# Baseline::Compare busy-waits forever. Only enabled for WebGPU; enabling it on WebGL causes
# function signature mismatch crashes in indirect calls.
if (PAG_USE_WEBGPU)
    # ASYNCIFY_REMOVE excludes the shader compilation libraries (tint and friends) from
    # instrumentation. These are pure CPU computation and never participate in async
    # operations; instrumenting them corrupts their execution state.
    set(ASYNCIFY_LINK_OPTIONS
            -sASYNCIFY=1
            -sASYNCIFY_STACK_SIZE=262144
            "-sASYNCIFY_REMOVE=['shaderc*','glslang*','spv::*','spvtools*','tint::*','Glslang*','TShader*','Compile*ToSpv*','SpvCompilat*']"
    )
    target_compile_definitions(pag-test PRIVATE PAG_USE_ASYNCIFY)
    set(WEB_GRAPHICS_LINK_OPTIONS -sUSE_WEBGPU=1)
    set(WEB_EXPORTED_RUNTIME_METHODS "-sEXPORTED_RUNTIME_METHODS=['FS','ccall','cwrap','WebGPU']")
else ()
    set(ASYNCIFY_LINK_OPTIONS "")
    set(WEB_GRAPHICS_LINK_OPTIONS -sMAX_WEBGL_VERSION=2)
    set(WEB_EXPORTED_RUNTIME_METHODS "-sEXPORTED_RUNTIME_METHODS=['FS','ccall','cwrap','GL']")
endif ()

target_compile_definitions(pag-test PRIVATE
        PAG_BACKEND_NAME="${PAG_BACKEND_NAME}"
        $<$<BOOL:${PAG_UPDATE_BASELINE}>:UPDATE_BASELINE>
        $<$<NOT:$<BOOL:${PAG_SKIP_GENERATE_BASELINE_IMAGES}>>:GENERATE_BASELINE_IMAGES>
)

target_link_options(pag-test PRIVATE
        -lembind
        -fno-rtti
        -DEMSCRIPTEN_HAS_UNBOUND_TYPE_NAMES=0
        -sWASM=1
        ${WEB_GRAPHICS_LINK_OPTIONS}
        ${ASYNCIFY_LINK_OPTIONS}
        -sALLOW_MEMORY_GROWTH=1
        -sINITIAL_MEMORY=256MB
        -sMAXIMUM_MEMORY=2GB
        -sSTACK_SIZE=1MB
        -sEXPORT_NAME='PAGTest'
        -sEXPORTED_FUNCTIONS=['_RunAllTests','_RunTest']
        ${WEB_EXPORTED_RUNTIME_METHODS}
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sENVIRONMENT=web,worker
        "SHELL:--preload-file ${ROOT}/test/baseline/version.json@/test/baseline/version.json"
        "SHELL:--preload-file ${CMAKE_CURRENT_BINARY_DIR}/HEAD@/test/baseline/.cache/${PAG_BACKEND_NAME}/HEAD"
        ${PRELOAD_CACHE}
        ${PRELOAD_RESOURCES}
)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(pag-test PRIVATE -g3 -O0)
    target_link_options(pag-test PRIVATE
            -g3 -O0
            -gsource-map
            -sASSERTIONS=2
            -Wno-limited-postlink-optimizations
    )
else ()
    target_compile_options(pag-test PRIVATE -Oz)
    target_link_options(pag-test PRIVATE -Oz)
endif ()

target_include_directories(pag-test PRIVATE
        ${ROOT}                                   # pagx/... headers used by the pagx suites
        ${ROOT}/src                               # libpag internal headers (base/..., rendering/...)
        ${ROOT}/test/src                          # test utilities (utils/..., base/...)
        ${TGFX_DIR}/src                           # tgfx internal headers (core/utils/MD5.h, AtlasManager.h)
        ${TGFX_DIR}/include
        ${TGFX_DIR}/third_party/googletest/googletest
        ${TGFX_DIR}/third_party/googletest/googletest/include
        ${TGFX_DIR}/third_party/json/include
        ${MINIZIP_DIR}
        ${WOFF2_DIR}/include
        ${BROTLI_DIR}/c/include
)

target_link_libraries(pag-test
        pag
        ${TEST_VENDOR_LIBS}
)
