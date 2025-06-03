cmake_minimum_required(VERSION 3.13)
project(PAGViewer)

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

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# configures the local installation path of the QT library.
if (NOT CMAKE_PREFIX_PATH)
    if (NOT EXISTS ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg)
        file(WRITE ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg
                "set(CMAKE_PREFIX_PATH /Users/[username]/Qt/6.6.1/macos/lib/cmake)  #put your own QT path here")
    endif ()
    include("./QTCMAKE.cfg")
endif ()

string(REGEX MATCH "([0-9]+)\\.[0-9]+\\.[0-9]+" QT_VERSION ${CMAKE_PREFIX_PATH})
if (QT_VERSION)
    string(REGEX MATCH "^[0-9]+" QT_VERSION_MAJOR ${QT_VERSION})
    if (QT_VERSION_MAJOR GREATER_EQUAL 6 AND CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(FATAL_ERROR "QT has dropped support for 32-bit builds since version 6.0.0")
    endif ()
endif ()

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets OpenGL Qml Quick QuickControls2)
list(APPEND PAG_VIEWER_PLATFORM_LIBS Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::QuickControls2)
qt_add_resources(QT_RESOURCES res.qrc)

message(STATUS "Generate Version File")
set(MajorVersion $ENV{BK_CI_MAJOR_VERSION} 1)
set(MinorVersion $ENV{BK_CI_MINOR_VERSION} 0)
set(BuildNumber $ENV{BK_CI_BUILD_NO} 0)
if ($ENV{isBetaVersion} MATCHES "false")
    set(UpdateChannel "stable")
else ()
    set(UpdateChannel "beta")
endif ()
configure_file(${CMAKE_SOURCE_DIR}/package/templates/version.h.in
    ${CMAKE_SOURCE_DIR}/src/version.h
    @ONLY
)

message(STATUS "Build Version: ${MajorVersion}.${MinorVersion}.${BuildNumber}")
message(STATUS "Update Channel: ${UpdateChannel}")

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
    find_library(COMPRESSION_LIBRARIES NAMES compression)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${COMPRESSION_LIBRARIES})
elseif (WIN32)
    set(BUILD_USE_64BITS ON)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES)
    find_library(Bcrypt_LIB Bcrypt)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${Bcrypt_LIB})
    find_library(ws2_32_LIB ws2_32)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${ws2_32_LIB})
endif ()

set(PAG_USE_QT ON)
set(PAG_USE_RTTR ON)
set(PAG_USE_LIBAVC OFF)
set(PAG_BUILD_SHARED OFF)
set(PAG_BUILD_FRAMEWORK OFF)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ ./libpag)

set(RTTR_OUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/editing/rttr/PAGRttr.hpp")
# execute_process(COMMAND bash -c "${CMAKE_CURRENT_SOURCE_DIR}/sync_deps.sh")
execute_process(COMMAND bash -c "chmod +x ${CMAKE_CURRENT_SOURCE_DIR}/tools/RttrAutoRegister${CMAKE_EXECUTABLE_SUFFIX} && ${CMAKE_CURRENT_SOURCE_DIR}/tools/RttrAutoRegister${CMAKE_EXECUTABLE_SUFFIX} -s ${CMAKE_CURRENT_SOURCE_DIR}/../include/pag -o ${RTTR_OUT_PATH} -m PAG_API -i ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}")

# collects pag include directories.
set(PAG_VIEWER_INCLUDES ./ src ../ ../include ../src ../third_party/tgfx/include)
file(GLOB_RECURSE PAG_VIEWER_SOURCE_FILES src/*.*)
list(FILTER PAG_VIEWER_SOURCE_FILES EXCLUDE REGEX src/platform/*.*)
if (APPLE)
    list(APPEND PAG_VIEWER_INCLUDES src/platform/mac)
    file(GLOB_RECURSE PLATFORM_SOURCE_FILES src/platform/mac/*.*)
elseif (WIN32)
    list(APPEND PAG_VIEWER_INCLUDES src/platform/win)
    file(GLOB_RECURSE PLATFORM_SOURCE_FILES src/platform/win/*.*)
endif ()
list(APPEND PAG_VIEWER_SOURCE_FILES ${PLATFORM_SOURCE_FILES})

add_executable(PAGViewer ${RC_FILES} ${PAG_VIEWER_SOURCE_FILES} ${QT_RESOURCES})
if (APPLE)
    list(APPEND PAG_VIEWER_INCLUDES ../third_party/out/rttr/mac/include)
    list(APPEND PAG_VIEWER_INCLUDES ../third_party/tgfx/third_party/out/zlib/mac/include)
    list(APPEND PAG_VIEWER_INCLUDES ../third_party/tgfx/third_party/out/libpng/mac/include)
    list(APPEND VIEWER_VENDOR_LIBRARIES ${CMAKE_SOURCE_DIR}/vendor/sparkle/mac/Sparkle.framework)
elseif (WIN32)
    list(APPEND PAG_VIEWER_INCLUDES ../third_party/out/rttr/win/include)
    list(APPEND PAG_VIEWER_INCLUDES ../third_party/tgfx/third_party/out/zlib/win/include)
    list(APPEND PAG_VIEWER_INCLUDES ../third_party/tgfx/third_party/out/libpng/win/include)
    list(APPEND PAG_VIEWER_INCLUDES ${CMAKE_SOURCE_DIR}/vendor/winsparkle/include)
    list(APPEND VIEWER_VENDOR_LIBRARIES ${CMAKE_SOURCE_DIR}/vendor/winsparkle/win/x64/${CMAKE_BUILD_TYPE}/WinSparkle.lib)
endif ()

target_include_directories(PAGViewer PUBLIC ${PAG_VIEWER_INCLUDES})
target_include_directories(PAGViewer SYSTEM PRIVATE ./src/editing/rttr)
target_link_libraries(PAGViewer pag ${PAG_VIEWER_PLATFORM_LIBS} ${VIEWER_VENDOR_LIBRARIES})