# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(Hello2D)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wall -Wextra -Weffc++ -pedantic -Werror=return-type -Wno-unused-command-line-argument -Wno-implicit-int-float-conversion)
endif ()

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include
                    ${NATIVERENDER_ROOT_PATH}/third_party/tgfx/include)

set(TGFX_BUILD_DRAWERS ON)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../ ./libpag)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../ ./libpag)

file(GLOB_RECURSE HELLO_2D_FILES napi_init.cpp)
add_library(hello2d SHARED ${HELLO_2D_FILES})
target_link_libraries(hello2d libnative_media_codecbase.so libnative_media_core.so libnative_media_vdec.so
                              tgfx-drawers tgfx ${TGFX_SHARED_LIBS} pag)