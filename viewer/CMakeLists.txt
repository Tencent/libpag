cmake_minimum_required(VERSION 3.13)
project(PAGViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
message(STATUS "System Name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "System Processor: ${CMAKE_SYSTEM_PROCESSOR}")
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
endif()

if (WIN32)
    add_compile_options(-bigobj)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
endif ()

include(tools/vendor_tools/vendor.cmake)

add_vendor_target(log4qt STATIC_VENDORS log4qt)
add_vendor_target(nlohman-json STATIC_VENDORS nlohman-json)
add_vendor_target(sodium STATIC_VENDORS sodium)
add_vendor_target(sparkle STATIC_VENDORS sparkle)

add_executable(PAGViewer src/main.cpp)
add_dependencies(PAGViewer log4qt nlohman-json sodium sparkle)