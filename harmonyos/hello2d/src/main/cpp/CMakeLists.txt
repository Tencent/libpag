# the minimum version of CMake.
cmake_minimum_required(VERSION 3.4.1)
project(Hello2D)

set(NATIVERENDER_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})

if(DEFINED PACKAGE_FIND_FILE)
    include(${PACKAGE_FIND_FILE})
endif()

include_directories(${NATIVERENDER_ROOT_PATH}
                    ${NATIVERENDER_ROOT_PATH}/include)

find_library(EGL-lib EGL)
find_library(GLES-lib GLESv3)
find_library(libace-lib ace_ndk.z)
find_library(hilog-lib hilog_ndk.z)
add_library(hello2d SHARED napi_init.cpp egl_core.cpp)
target_link_libraries(hello2d PUBLIC ${EGL-lib} ${GLES-lib} ${libace-lib} ${hilog-lib} libace_napi.z.so)