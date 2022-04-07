cmake_minimum_required(VERSION 3.13)
project(PAGViewer)

if (APPLE AND NOT IOS)
    SET(CMAKE_SYSTEM_PROCESSOR x86_64)
    SET(CMAKE_OSX_ARCHITECTURES x86_64)
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#    add_definitions(-Werror -Wall -Wextra -Weffc++ -pedantic -Werror=return-type)
#endif ()

if (MSVC)
    add_compile_options("/utf-8")
endif (MSVC)

# Sets flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g -O1")
endif ()

add_definitions(-DQT_NO_KEYWORDS)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 配置 QT 库的本地安装路径
if (NOT EXISTS ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg)
    file(WRITE ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg
            "set(CMAKE_PREFIX_PATH /Users/username/Qt5.13.0/5.13.0/clang_64/lib/cmake)  #这里需要填入自己安装的路径")
endif ()
include("./QTCMAKE.cfg")

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5Qml REQUIRED)
find_package(Qt5Quick REQUIRED)
list(APPEND PAG_VIEWER_PLATFORM_LIBS Qt5::Widgets Qt5::OpenGL Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick)
add_definitions(${Qt5Widgets_DEFINITIONS} ${QtQml_DEFINITIwONS} ${${Qt5Quick_DEFINITIONS}})
qt5_add_resources(QT_RESOURCES res.qrc)

if (APPLE)
    find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices REQUIRED)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${APPLICATION_SERVICES_FRAMEWORK})
    find_library(QUARTZ_CORE QuartzCore REQUIRED)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${QUARTZ_CORE})
    find_library(COCOA Cocoa REQUIRED)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${COCOA})
    find_library(FOUNDATION Foundation REQUIRED)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${FOUNDATION})
    find_library(ICONV_LIBRARIES NAMES iconv libiconv libiconv-2 c)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${ICONV_LIBRARIES})
    find_library(VIDEOTOOLBOX VideoToolbox)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${VIDEOTOOLBOX})
    find_library(CORE_MEDIA CoreMedia)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${CORE_MEDIA})
elseif (WIN32)
    set(BUILD_USE_64BITS ON)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES)
    find_library(Bcrypt_LIB Bcrypt)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${Bcrypt_LIB})
    find_library(ws2_32_LIB ws2_32)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${ws2_32_LIB})
endif ()
# collects pag include directories.
set(PAG_VIEWER_INCLUDES ./ src ../ ../include ../src ../tgfx/include ../tgfx/src)
file(GLOB_RECURSE PAG_VIEWER_SOURCE_FILES src/*.*)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(PAG_USE_QT ON)
set(PAG_BUILD_SHARED OFF)

add_subdirectory(../ libpag EXCLUDE_FROM_ALL)

add_executable(PAGViewer MACOSX_BUNDLE ${RC_FILES} ${PAG_VIEWER_SOURCE_FILES} ${QT_RESOURCES})
target_include_directories(PAGViewer PUBLIC ${PAG_VIEWER_INCLUDES})
target_link_libraries(PAGViewer pag ${PAG_VIEWER_PLATFORM_LIBS})
qt5_use_modules(PAGViewer Core Widgets Gui OpenGL)