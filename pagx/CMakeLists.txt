cmake_minimum_required(VERSION 3.13)
project(PAGX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PAGX_BUILD_SVG "Build PAGX SVG parser module" ON)
option(PAGX_BUILD_TGFX_ADAPTER "Build PAGX to tgfx adapter (LayerBuilder)" ON)

# ============== Core PAGX Library (Independent, no tgfx dependency) ==============

# Core sources
file(GLOB PAGX_CORE_SOURCES
    src/PAGXNode.cpp
    src/PAGXTypes.cpp
    src/PAGXDocument.cpp
    src/PAGXXMLParser.cpp
    src/PAGXXMLWriter.cpp
    src/PathData.cpp
)
list(APPEND PAGX_SOURCES ${PAGX_CORE_SOURCES})

# SVG parser (also independent of tgfx)
if (PAGX_BUILD_SVG)
    file(GLOB PAGX_SVG_SOURCES src/svg/*.cpp)
    list(APPEND PAGX_SOURCES ${PAGX_SVG_SOURCES})
endif()

# tgfx adapter sources (requires tgfx)
if (PAGX_BUILD_TGFX_ADAPTER)
    if (NOT TARGET tgfx)
        message(FATAL_ERROR "tgfx target not found. Set PAGX_BUILD_TGFX_ADAPTER=OFF to build without tgfx adapter.")
    endif()
    file(GLOB PAGX_TGFX_SOURCES src/tgfx/*.cpp)
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
