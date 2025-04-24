cmake_minimum_required(VERSION 3.13)
project(ae_sdk_util)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(
        Util/
        Headers/
        Headers/SP/
)

file(GLOB UTIL_SOURCES "Util/*.cpp")

add_library(ae_sdk_util STATIC ${UTIL_SOURCES})

set_target_properties(ae_sdk_util PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)