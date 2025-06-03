cmake_minimum_required(VERSION 3.13)
project(PAGExporter)

include(../third_party/vendor_tools/vendor.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_definitions(-Werror -Wall -Wextra -Weffc++ -pedantic -Werror=return-type -fno-rtti)
endif ()

# configures the local installation path of the AE_SDK.
if (NOT AE_SDK_PATH)
    if (NOT EXISTS ${PROJECT_SOURCE_DIR}/AESDK.cfg)
        file(WRITE ${PROJECT_SOURCE_DIR}/AESDK.cfg
                "set(AE_SDK_PATH ${PROJECT_SOURCE_DIR}/vendor/AfterEffectsSDK/Examples)  #put your own AE_SDK path here")
    endif ()
    include("./AESDK.cfg")
endif ()

# configures the local installation path of the QT library.
if (NOT CMAKE_PREFIX_PATH)
    if (NOT EXISTS ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg)
        file(WRITE ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg
                "set(CMAKE_PREFIX_PATH /Users/[username]/Qt/6.6.1/macos/lib/cmake)  #put your own QT path here")
    endif ()
    include("./QTCMAKE.cfg")
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
