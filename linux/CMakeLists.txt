cmake_minimum_required(VERSION 3.5)
message("--begin complie---")
project(pag-linux)
set(CMAKE_CXX_STANDARD 11)

if (APPLE AND NOT IOS)
    SET(CMAKE_SYSTEM_PROCESSOR x86_64)
    SET(CMAKE_OSX_ARCHITECTURES x86_64)
endif ()

add_definitions(-DSWIFTSHADER)
add_definitions(-DGL_GLEXT_PROTOTYPES)
add_definitions(-DEGL_EGLEXT_PROTOTYPES)

add_executable(pag-linux src/main.cpp)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/vendor/pag/include
        src)

if (APPLE)
    set(CMAKE_CXX_FLAGS "-stdlib=libc++")

    find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices REQUIRED)
    list(APPEND libs ${APPLICATION_SERVICES_FRAMEWORK})
    find_library(QUARTZ_CORE QuartzCore REQUIRED)
    list(APPEND libs ${QUARTZ_CORE})
    find_library(COCOA Cocoa REQUIRED)
    list(APPEND libs ${COCOA})
    find_library(FOUNDATION Foundation REQUIRED)
    list(APPEND libs ${FOUNDATION})
    find_library(VIDEOTOOLBOX VideoToolbox)
    list(APPEND libs ${VIDEOTOOLBOX})

    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/pag/mac/x64/libpag.dylib)
    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/swiftshader/mac/x64/libEGL.dylib)
    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/swiftshader/mac/x64/libGLESv2.dylib)

else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${cppFlags} -D_GLIBCXX_USE_CXX11_ABI=0")
    add_compile_options(-fPIC)
    add_compile_options(-pthread)
    add_definitions(-D__STDC_FORMAT_MACROS)
    SET(CMAKE_EXE_LINKER_FLAGS " -no-pie")

    find_package(Threads)
    list(APPEND libs ${CMAKE_THREAD_LIBS_INIT})
    find_package(ZLIB)
    include_directories(${ZLIB_INCLUDE_DIRS})
    list(APPEND libs ${ZLIB_LIBRARIES})
    list(APPEND libs dl)

    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/pag/linux/x64/libpag.a)
    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/swiftshader/linux/x64/libEGL.so)
    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/swiftshader/linux/x64/libGLESv2.so)

endif()

target_link_libraries(pag-linux ${libs})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g -O1")
endif()

