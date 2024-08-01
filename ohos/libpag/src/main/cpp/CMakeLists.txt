# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(myNpmLib)
set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
add_definitions(-DOHOS_PLATFORM)
set(PAG_BUILD_SHARED OFF)

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)


add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../ ./libpag)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../src ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../include  ${CMAKE_CURRENT_SOURCE_DIR}/../../../../../third_party/tgfx/include)

file(GLOB_RECURSE LIBPAG_OHOS_SRC_FILES *.cpp)
add_library(libpag SHARED ${LIBPAG_OHOS_SRC_FILES})
target_link_libraries(libpag PUBLIC pag)