cmake_minimum_required(VERSION 3.13)
project(PAGX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PAGX_BUILD_SVG "Build PAGX SVG parser module" ON)
option(PAGX_BUILD_TGFX_ADAPTER "Build PAGX to tgfx adapter (LayerBuilder)" ON)

# ============== tgfx dependency (when building standalone) ==============
if (PAGX_BUILD_TGFX_ADAPTER AND NOT TARGET tgfx)
    if (NOT TGFX_DIR)
        set(TGFX_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/tgfx)
    endif ()
    get_filename_component(TGFX_DIR "${TGFX_DIR}" REALPATH)
    list(APPEND PAGX_INCLUDES ${TGFX_DIR}/include)
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
        list(APPEND PAGX_STATIC_LIBS ${TGFX_OUTPUT_LIB})
        list(APPEND PAGX_DEPEND_TARGETS tgfx)
    else ()
        # Fallback to source build
        message("TGFX_DIR: ${TGFX_DIR}")
        set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
        set(TGFX_BUILD_SVG ON CACHE BOOL "" FORCE)
        set(TGFX_BUILD_LAYERS ON CACHE BOOL "" FORCE)
        add_subdirectory(${TGFX_DIR} ${CMAKE_CURRENT_BINARY_DIR}/tgfx)
        list(APPEND PAGX_STATIC_LIBS $<TARGET_FILE:tgfx>)
        list(APPEND PAGX_DEPEND_TARGETS tgfx)
    endif ()
endif ()

# ============== Core PAGX Library (Independent, no tgfx dependency) ==============

# Collect core sources (excluding subdirectories)
file(GLOB PAGX_CORE_SOURCES src/*.cpp)
list(APPEND PAGX_SOURCES ${PAGX_CORE_SOURCES})

# XML parser sources (based on expat)
file(GLOB_RECURSE PAGX_XML_SOURCES src/xml/*.cpp)
list(APPEND PAGX_SOURCES ${PAGX_XML_SOURCES})

# SVG parser (also independent of tgfx)
if (PAGX_BUILD_SVG)
    file(GLOB_RECURSE PAGX_SVG_SOURCES src/svg/*.cpp)
    list(APPEND PAGX_SOURCES ${PAGX_SVG_SOURCES})
endif ()

# tgfx adapter sources (requires tgfx)
if (PAGX_BUILD_TGFX_ADAPTER)
    file(GLOB_RECURSE PAGX_TGFX_SOURCES src/tgfx/*.cpp)
    list(APPEND PAGX_SOURCES ${PAGX_TGFX_SOURCES})
endif ()

add_library(pagx STATIC ${PAGX_SOURCES})

# Public include directory
target_include_directories(pagx PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Private include directories
target_include_directories(pagx PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${PAGX_INCLUDES}
)

# expat include directory
target_include_directories(pagx PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/expat/expat/lib
)

# Windows requires XML_STATIC definition for static expat
if (WIN32)
    target_compile_definitions(pagx PRIVATE XML_STATIC)
endif ()

if (PAGX_BUILD_SVG)
    target_include_directories(pagx PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/svg
    )
endif ()

if (PAGX_BUILD_TGFX_ADAPTER)
    target_include_directories(pagx PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/tgfx
    )
    if (TARGET tgfx)
        target_link_libraries(pagx PUBLIC tgfx)
    endif ()
    target_link_libraries(pagx PUBLIC ${PAGX_STATIC_LIBS})
endif ()

if (PAGX_DEPEND_TARGETS)
    add_dependencies(pagx ${PAGX_DEPEND_TARGETS})
endif ()

# ============== Vendor (expat) ==============
# Build expat and merge into pagx static library
add_vendor_target(pagx-vendor STATIC_VENDORS expat CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
find_vendor_libraries(pagx-vendor STATIC PAGX_VENDOR_STATIC_LIBRARIES)
add_dependencies(pagx pagx-vendor)
merge_libraries_into(pagx ${PAGX_VENDOR_STATIC_LIBRARIES})
