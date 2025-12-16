cmake_minimum_required(VERSION 3.13)
project(PAGViewer)

include(../third_party/vendor_tools/vendor.cmake)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif ()

if (CMAKE_OSX_ARCHITECTURES MATCHES "arm64" OR CMAKE_OSX_ARCHITECTURES MATCHES "aarch64")
    set(ARCH arm64)
elseif (CMAKE_OSX_ARCHITECTURES MATCHES "x86_64" OR CMAKE_OSX_ARCHITECTURES MATCHES "amd64")
    set(ARCH x64)
endif ()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_definitions(-Werror -Wall -Wextra -Weffc++ -pedantic -Werror=return-type -fno-rtti)
endif ()

if (MSVC)
    add_compile_options("/utf-8" /GR /bigobj)
endif (MSVC)

# Sets flags
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions(-DDEBUG)
endif ()

add_definitions(-DQT_NO_KEYWORDS -DPAG_USE_RTTR)

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
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets OpenGL Qml Quick QuickControls2 Multimedia)
list(APPEND PAG_VIEWER_PLATFORM_LIBS Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::OpenGL Qt${QT_VERSION_MAJOR}::Qml Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::QuickControls2 Qt${QT_VERSION_MAJOR}::Multimedia)
find_program(LUPDATE_EXECUTABLE lupdate
        PATHS ${CMAKE_PREFIX_PATH}/../../bin
        REQUIRED)
find_program(LRELEASE_EXECUTABLE lrelease
        PATHS ${CMAKE_PREFIX_PATH}/../../bin
        REQUIRED)
execute_process(COMMAND ${LUPDATE_EXECUTABLE} -recursive ${PROJECT_SOURCE_DIR}/src ${PROJECT_SOURCE_DIR}/assets/qml -ts ${PROJECT_SOURCE_DIR}/assets/translation/Chinese.ts)
execute_process(COMMAND ${LRELEASE_EXECUTABLE} ${PROJECT_SOURCE_DIR}/assets/translation/Chinese.ts -qm ${PROJECT_SOURCE_DIR}/assets/translation/Chinese.qm)
qt_add_resources(QT_RESOURCES assets/res.qrc)

message(STATUS "Generate Version File")
if (DEFINED ENV{MajorVersion})
    set(MajorVersion "$ENV{MajorVersion}")
else ()
    set(MajorVersion "1")
endif()
if (DEFINED ENV{MinorVersion})
    set(MinorVersion "$ENV{MinorVersion}")
else ()
    set(MinorVersion "0")
endif ()
if (DEFINED ENV{BuildNumber})
    set(BuildNumber "$ENV{BuildNumber}")
else()
    set(BuildNumber "0")
endif ()
if ($ENV{isBetaVersion} MATCHES false)
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
    find_library(version_LIB version)
    list(APPEND PAG_VIEWER_PLATFORM_LIBS ${version_LIB})
endif ()

set(PAG_USE_QT ON)
set(PAG_USE_RTTR ON)
set(PAG_USE_LIBAVC OFF)
set(PAG_BUILD_TESTS OFF)
set(PAG_USE_HARFBUZZ ON)
set(PAG_BUILD_SHARED OFF)
set(PAG_BUILD_FRAMEWORK OFF)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
list(APPEND PAG_OPTIONS "-DPAG_USE_QT=${PAG_USE_QT}")
list(APPEND PAG_OPTIONS "-DPAG_USE_RTTR=${PAG_USE_RTTR}")
list(APPEND PAG_OPTIONS "-DPAG_USE_LIBAVC=${PAG_USE_LIBAVC}")
list(APPEND PAG_OPTIONS "-DPAG_BUILD_TESTS=${PAG_BUILD_TESTS}")
list(APPEND PAG_OPTIONS "-DPAG_USE_HARFBUZZ=${PAG_USE_HARFBUZZ}")
list(APPEND PAG_OPTIONS "-DPAG_BUILD_SHARED=${PAG_BUILD_SHARED}")
list(APPEND PAG_OPTIONS "-DPAG_BUILD_FRAMEWORK=${PAG_BUILD_FRAMEWORK}")
list(APPEND PAG_OPTIONS "-DCMAKE_CXX_FLAGS=\"-DPAG_USE_FFAVC -I${CMAKE_SOURCE_DIR}/vendor/ffmovie/include\"")
list(APPEND PAG_OPTIONS "-DCMAKE_PREFIX_PATH=\"${CMAKE_PREFIX_PATH}\"")
if (NOT DEFINED PAG_PATH OR "${PAG_PATH}" STREQUAL "")
    set(PAG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/..")
    get_filename_component(PAG_PATH "${PAG_PATH}" ABSOLUTE)
endif ()
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    list(APPEND PAG_OPTIONS "-d")
    string(TOLOWER ${CMAKE_BUILD_TYPE} TGFX_BUILD_TYPE)
endif ()
set(PAG_OUTPUT_DIR ${CMAKE_BINARY_DIR}/libpag)
set(PAG_OUTPUT_LIB ${PAG_OUTPUT_DIR}/${ARCH}/libpag${CMAKE_STATIC_LIBRARY_SUFFIX})
set(PAG_OUTPUT_MD5 ${PAG_OUTPUT_DIR}/.pag.${ARCH}.md5)
add_custom_target(pag
        COMMAND node "${PAG_PATH}/build_pag" ${PAG_OPTIONS} -p ${PLATFORM} -a ${ARCH} -o ${PAG_OUTPUT_DIR}
        WORKING_DIRECTORY ${PAG_PATH}
        BYPRODUCTS ${PAG_OUTPUT_LIB} ${PAG_OUTPUT_MD5}
        VERBATIM USES_TERMINAL)
list(APPEND PAG_VIEWER_PLATFORM_LIBS ${PAG_OUTPUT_LIB})
list(APPEND PAG_DEPEND_TARGETS pag)

execute_process(COMMAND bash -c "${CMAKE_CURRENT_SOURCE_DIR}/sync_deps.sh")
if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/RttrAutoRegister${CMAKE_EXECUTABLE_SUFFIX}")
    set(RTTR_OUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/src/editing/rttr/PAGRttr.hpp")
    execute_process(COMMAND bash -c "chmod +x ${CMAKE_CURRENT_SOURCE_DIR}/tools/RttrAutoRegister${CMAKE_EXECUTABLE_SUFFIX} && ${CMAKE_CURRENT_SOURCE_DIR}/tools/RttrAutoRegister${CMAKE_EXECUTABLE_SUFFIX} -s ${CMAKE_CURRENT_SOURCE_DIR}/../include/pag -o ${RTTR_OUT_PATH} -m PAG_API -i ${CMAKE_CURRENT_SOURCE_DIR}/../include ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES}")
endif()

# collects pag include directories.
set(PAG_VIEWER_INCLUDES ./ src ../ ../include ../src ../third_party/tgfx/include)
set(PAG_VIEWER_SYSTEM_INCLUDES ./src/editing/rttr)
file(GLOB_RECURSE PAG_VIEWER_SOURCE_FILES src/*.*)
list(FILTER PAG_VIEWER_SOURCE_FILES EXCLUDE REGEX src/platform/*.*)
list(APPEND PAG_VIEWER_INCLUDES src/platform)
if (APPLE)
    file(GLOB PLATFORM_SOURCE_FILES src/platform/*.* src/platform/mac/*.*)
elseif (WIN32)
    file(GLOB PLATFORM_SOURCE_FILES src/platform/*.* src/platform/win/*.*)
endif ()
list(APPEND PAG_VIEWER_SOURCE_FILES ${PLATFORM_SOURCE_FILES})

list(APPEND PAG_VIEWER_INCLUDES ${CMAKE_SOURCE_DIR}/vendor/sonic/include)
list(APPEND PAG_VIEWER_INCLUDES ${CMAKE_SOURCE_DIR}/vendor/ffmovie/include)
if (APPLE)
    add_executable(PAGViewer ${RC_FILES} ${PAG_VIEWER_SOURCE_FILES} ${QT_RESOURCES})
    list(APPEND PAG_VIEWER_SYSTEM_INCLUDES ../third_party/out/rttr/mac/include)
    list(APPEND PAG_VIEWER_INCLUDES ../third_party/tgfx/third_party/out/zlib/mac/include)
    list(APPEND PAG_VIEWER_INCLUDES ../third_party/tgfx/third_party/out/libpng/mac/include)
    list(APPEND VIEWER_VENDOR_LIBRARIES ${CMAKE_SOURCE_DIR}/vendor/sparkle/mac/Sparkle.framework)
    list(APPEND VIEWER_VENDOR_LIBRARIES ${CMAKE_SOURCE_DIR}/vendor/sonic/mac/${ARCH}/libsonic.a)
    list(APPEND VIEWER_VENDOR_LIBRARIES ${CMAKE_SOURCE_DIR}/vendor/ffmovie/mac/${ARCH}/libffmovie.dylib)
elseif (WIN32)
    set(RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/package/templates/PAGViewer.rc"
            "${CMAKE_CURRENT_SOURCE_DIR}/package/templates/appIcon.ico"
            "${CMAKE_CURRENT_SOURCE_DIR}/package/templates/pagIcon.ico")

    add_executable(PAGViewer WIN32 ${RC_FILES} ${PAG_VIEWER_SOURCE_FILES} ${QT_RESOURCES} ${RESOURCES})
    list(APPEND PAG_VIEWER_SYSTEM_INCLUDES ../third_party/out/rttr/win/include)
    list(APPEND PAG_VIEWER_INCLUDES ../third_party/tgfx/third_party/out/zlib/win/include)
    list(APPEND PAG_VIEWER_INCLUDES ../third_party/tgfx/third_party/out/libpng/win/include)
    list(APPEND PAG_VIEWER_INCLUDES ${CMAKE_SOURCE_DIR}/vendor/winsparkle/include)
    list(APPEND VIEWER_VENDOR_LIBRARIES ${CMAKE_SOURCE_DIR}/vendor/winsparkle/win/x64/${CMAKE_BUILD_TYPE}/WinSparkle.lib)
    list(APPEND VIEWER_VENDOR_LIBRARIES ${CMAKE_SOURCE_DIR}/vendor/sonic/win/${ARCH}/sonic.lib)
    list(APPEND VIEWER_VENDOR_LIBRARIES ${CMAKE_SOURCE_DIR}/vendor/ffmovie/win/${ARCH}/ffmovie.lib)
endif ()

if (PAG_DEPEND_TARGETS)
    add_dependencies(PAGViewer ${PAG_DEPEND_TARGETS})
endif ()

target_include_directories(PAGViewer PUBLIC ${PAG_VIEWER_INCLUDES})
target_include_directories(PAGViewer SYSTEM PRIVATE ${PAG_VIEWER_SYSTEM_INCLUDES})
target_link_libraries(PAGViewer ${PAG_VIEWER_PLATFORM_LIBS} ${VIEWER_VENDOR_LIBRARIES})