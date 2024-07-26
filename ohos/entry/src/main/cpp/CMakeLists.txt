# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(ohos)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DOHOS_PLATFORM)

file(GLOB_RECURSE LIBPAG_OHOS_SRC_FILES *.cpp)
add_library(testnative SHARED ${LIBPAG_OHOS_SRC_FILES})
target_link_libraries(testnative PUBLIC libace_napi.z.so libace_napi.z.so
                                                          libace_ndk.z.so
                                                          libnative_window.so
                      )