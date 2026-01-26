cmake_minimum_required(VERSION 3.13)
project(PAGX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PAGX_BUILD_SVG "Build PAGX SVG parser module" ON)
option(PAGX_BUILD_TGFX_ADAPTER "Build PAGX to tgfx adapter (LayerBuilder)" ON)

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
endif()

# tgfx adapter sources (requires tgfx)
if (PAGX_BUILD_TGFX_ADAPTER)
    if (NOT TARGET tgfx)
        message(FATAL_ERROR "tgfx target not found. Set PAGX_BUILD_TGFX_ADAPTER=OFF to build without tgfx adapter.")
    endif()
    file(GLOB_RECURSE PAGX_TGFX_SOURCES src/tgfx/*.cpp)
    list(APPEND PAGX_SOURCES ${PAGX_TGFX_SOURCES})
endif()

add_library(pagx STATIC ${PAGX_SOURCES})

# Public include directory
target_include_directories(pagx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Private include directories
target_include_directories(pagx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# expat include directory
target_include_directories(pagx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../third_party/expat/expat/lib
)

# Windows requires XML_STATIC definition for static expat
if(WIN32)
    target_compile_definitions(pagx PRIVATE XML_STATIC)
endif()

if (PAGX_BUILD_SVG)
    target_include_directories(pagx PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/svg
    )
endif()

if (PAGX_BUILD_TGFX_ADAPTER)
    target_include_directories(pagx PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/tgfx
    )
    target_link_libraries(pagx PUBLIC tgfx)
endif()

# ============== Vendor (expat) ==============
# Build expat and merge into pagx static library
add_vendor_target(pagx-vendor STATIC_VENDORS expat CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
find_vendor_libraries(pagx-vendor STATIC PAGX_VENDOR_STATIC_LIBRARIES)
add_dependencies(pagx pagx-vendor)
merge_libraries_into(pagx ${PAGX_VENDOR_STATIC_LIBRARIES})
