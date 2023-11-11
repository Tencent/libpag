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

# configures the local installation path of the QT library.
if (NOT CMAKE_PREFIX_PATH)
    if (NOT EXISTS ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg)
        file(WRITE ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg
                "set(CMAKE_PREFIX_PATH /Users/username/Qt5.13.0/5.13.0/clang_64/lib/cmake)  #put your own QT path here")
    endif ()
    include("./QTCMAKE.cfg")
endif ()

find_package(Qt5Widgets REQUIRED)
find_package(Qt5Core REQUIRED)
find_package(Qt5Gui REQUIRED)
find_package(Qt5OpenGL REQUIRED)
find_package(Qt5Qml REQUIRED)
find_package(Qt5Quick REQUIRED)
list(APPEND PAG_VIEWER_PLATFORM_LIBS Qt5::Widgets Qt5::OpenGL Qt5::Core Qt5::Gui Qt5::Qml Qt5::Quick)
add_definitions(${Qt5Widgets_DEFINITIONS} ${QtQml_DEFINITIwONS} ${${Qt5Quick_DEFINITIONS}})
qt5_add_resources(QT_RESOURCES res.qrc)

if (MSVC)
    string(TOLOWER ${MSVC_C_ARCHITECTURE_ID} ARCH)
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64" OR $CMAKE_SYSTEM_PROCESSOR STREQUAL "amd64")
    set(ARCH x64)
elseif (CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(ARCH arm64)
else ()
    set(ARCH x86)
endif ()

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

# collects pag include directories.
set(PAG_VIEWER_INCLUDES ./ src ../ ../include ../src ../third_party/tgfx/include)
file(GLOB_RECURSE PAG_VIEWER_SOURCE_FILES src/*.*)

list(APPEND PAG_OPTIONS "-DPAG_USE_QT=ON")
list(APPEND PAG_OPTIONS "-DPAG_BUILD_SHARED=OFF")
list(APPEND PAG_OPTIONS "-DPAG_BUILD_FRAMEWORK=OFF")
list(APPEND PAG_OPTIONS "-DCMAKE_PREFIX_PATH=\"${CMAKE_PREFIX_PATH}\"")

string(TOLOWER ${CMAKE_BUILD_TYPE} PAG_BUILD_TYPE)
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    list(APPEND PAG_OPTIONS "-d")
endif ()
set(PAG_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../out/${PAG_BUILD_TYPE}/qt)
set(PAG_OUTPUT_LIB ${PAG_OUTPUT_DIR}/${ARCH}/libpag${CMAKE_STATIC_LIBRARY_SUFFIX})
set(PAG_OUTPUT_MD5 ${PAG_OUTPUT_DIR}/.pag.${ARCH}.md5)
add_custom_command(OUTPUT libpag
        COMMAND node build_pag ${PAG_OPTIONS} -a ${ARCH} -o ${PAG_OUTPUT_DIR}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../"
        BYPRODUCTS ${PAG_OUTPUT_LIB} ${PAG_OUTPUT_MD5}
        VERBATIM USES_TERMINAL)
list(APPEND PAG_VIEWER_PLATFORM_LIBS ${PAG_OUTPUT_LIB})

add_executable(PAGViewer libpag ${RC_FILES} ${PAG_VIEWER_SOURCE_FILES} ${QT_RESOURCES})
target_include_directories(PAGViewer PUBLIC ${PAG_VIEWER_INCLUDES})
target_link_libraries(PAGViewer ${PAG_VIEWER_PLATFORM_LIBS})
qt5_use_modules(PAGViewer Core Widgets Gui OpenGL)