# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(ohos)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DOHOS_PLATFORM)

include_directories(${NATIVERENDER_ROOT_PATH}
                              ${NATIVERENDER_ROOT_PATH}/include)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../ ./libpag)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../ ./libpag)

add_library(entry SHARED napi_init.cpp)
target_link_libraries(entry PUBLIC libace_napi.z.so pag
                      )