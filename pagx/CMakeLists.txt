cmake_minimum_required(VERSION 3.13)
project(PAGX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PAGX_BUILD_SVG "Build PAGX SVG converter module" ON)

# tgfx must be available as a target (added by parent project or find_package)
if (NOT TARGET tgfx)
    message(FATAL_ERROR "tgfx target not found. Please add pagx as a subdirectory of a project that includes tgfx.")
endif()

# PAGX library sources
file(GLOB PAGX_LAYERS_SOURCES src/layers/*.cpp)
list(APPEND PAGX_SOURCES ${PAGX_LAYERS_SOURCES})

if (PAGX_BUILD_SVG)
    file(GLOB PAGX_SVG_SOURCES src/svg/*.cpp)
    list(APPEND PAGX_SOURCES ${PAGX_SVG_SOURCES})
endif()

add_library(pagx STATIC ${PAGX_SOURCES})

target_include_directories(pagx PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_include_directories(pagx PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/layers
)

if (PAGX_BUILD_SVG)
    target_include_directories(pagx PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/svg
    )
endif()

target_link_libraries(pagx PUBLIC tgfx)
