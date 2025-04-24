cmake_minimum_required(VERSION 3.13)
project(PAGExporter)

message("CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}")
include(../third_party/vendor_tools/vendor.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_definitions(-Werror -Wall -Wextra -Weffc++ -pedantic -Werror=return-type -fno-rtti)
endif ()

if (MSVC)
    add_compile_options("/utf-8")
    add_compile_options(/GR-)
endif (MSVC)

# Sets flags
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions(-DDEBUG)
endif ()

add_definitions(-DQT_NO_KEYWORDS)

list(APPEND PAGExporter_STATIC_VENDORS AE_SDK ffmpeg)

add_vendor_target(PAGExporter-vendor STATIC_VENDORS ${PAGExporter_STATIC_VENDORS})
find_vendor_libraries(PAGExporter-vendor STATIC PAGExporter_VENDOR_STATIC_LIBRARIES)