cmake_minimum_required(VERSION 3.13)
project(PAGExporter)

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
elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.30)
    add_definitions(-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR=1)
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
    add_compile_options(/bigobj)
endif (MSVC)

# Sets flags
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    add_definitions(-DDEBUG)
endif ()

add_definitions(-DQT_NO_KEYWORDS)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Core)
find_package(Qt6 REQUIRED COMPONENTS Core Qml Quick Widgets OpenGL QuickControls2 LinguistTools)
list(APPEND libs Qt6::Core Qt6::Qml Qt6::Quick Qt::Widgets Qt6::OpenGL Qt6::QuickControls2)
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
configure_file(${CMAKE_SOURCE_DIR}/templates/version.h.in
        ${CMAKE_SOURCE_DIR}/src/version.h
        @ONLY
)

list(APPEND PAG_EXPORTER_INCLUDES ./ src ../ ../include ../src ../third_party/tgfx/include)
file(GLOB_RECURSE PAG_EXPORTER_SOURCE_FILES src/*.*)
file(GLOB_RECURSE PAG_EXPORTER_THIRD_PARTY_SOURCE_FILES
        ${AE_SDK_PATH}/Util/*.cpp
        vendor/TinyXML2/*.cpp)

list(FILTER PAG_EXPORTER_SOURCE_FILES EXCLUDE REGEX src/platform/*.*)
list(APPEND PAG_EXPORTER_SOURCE_FILES src/platform/PAGViewerInstaller.cpp)
if (APPLE)
    file(GLOB_RECURSE PLATFORM_SOURCE_FILES src/platform/mac/*.*)
elseif (WIN32)
    file(GLOB_RECURSE PLATFORM_SOURCE_FILES src/platform/win/*.*)
endif ()
list(APPEND PAG_EXPORTER_SOURCE_FILES ${PLATFORM_SOURCE_FILES})

if(MSVC)
    set_source_files_properties(${PAG_EXPORTER_THIRD_PARTY_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "/wd4100")
else()
    set_source_files_properties(${PAG_EXPORTER_THIRD_PARTY_SOURCE_FILES} PROPERTIES COMPILE_FLAGS "-Wno-unused-parameter -Wno-unused-variable")
endif()

list(APPEND PAG_EXPORTER_THIRD_PARTY_INCLUDES
        ${AE_SDK_PATH}/Headers
        ${AE_SDK_PATH}/Headers/SP
        ${AE_SDK_PATH}/Util
        ${AE_SDK_PATH}/Resources
        ../third_party/tgfx/include
        ../third_party/tgfx/third_party/json/include
        ../third_party/tgfx/third_party/libwebp/src
        vendor/TinyXML2
        vendor/ffaudio/include)

if (APPLE)
    find_library(COCOA Cocoa REQUIRED)
    list(APPEND libs ${COCOA})
    find_library(QUARTZ_CORE QuartzCore REQUIRED)
    list(APPEND libs ${QUARTZ_CORE})
    find_library(FOUNDATION Foundation REQUIRED)
    list(APPEND libs ${FOUNDATION})
    find_library(ICONV_LIBRARIES NAMES iconv libiconv libiconv-2 c)
    list(APPEND libs ${ICONV_LIBRARIES})
    find_library(A_G_L AGL REQUIRED)
    list(APPEND libs ${A_G_L})
    find_library(OPENGL OpenGL REQUIRED)
    list(APPEND libs ${OPENGL})
    find_library(VIDEO_TOOL_BOX VideoToolBox)
    list(APPEND libs ${VIDEO_TOOL_BOX})
    find_library(CORE_MEDIA CoreMedia)
    list(APPEND libs ${CORE_MEDIA})
    find_library(ICONV_LIBRARIES NAMES iconv libiconv libiconv-2 c)
    list(APPEND libs ${ICONV_LIBRARIES})
    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ffaudio/mac/${ARCH}/libffaudio.dylib)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)
elseif (WIN32)
    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ffaudio/win/x64/ffaudio.lib)
endif ()

set(PAG_USE_QT ON)
set(PAG_USE_RTTR OFF)
set(PAG_USE_LIBAVC ON)
set(PAG_BUILD_SHARED OFF)
set(PAG_BUILD_TESTS OFF)
set(PAG_BUILD_FRAMEWORK OFF)
set(TGFX_BUILD_LAYERS ON)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
list(APPEND PAG_OPTIONS "-DPAG_USE_QT=${PAG_USE_QT}")
list(APPEND PAG_OPTIONS "-DPAG_USE_RTTR=${PAG_USE_RTTR}")
list(APPEND PAG_OPTIONS "-DPAG_USE_LIBAVC=${PAG_USE_LIBAVC}")
list(APPEND PAG_OPTIONS "-DTGFX_BUILD_LAYERS=${TGFX_BUILD_LAYERS}")
list(APPEND PAG_OPTIONS "-DPAG_BUILD_SHARED=${PAG_BUILD_SHARED}")
list(APPEND PAG_OPTIONS "-DPAG_BUILD_TESTS=${PAG_BUILD_TESTS}")
list(APPEND PAG_OPTIONS "-DPAG_BUILD_FRAMEWORK=${PAG_BUILD_FRAMEWORK}")
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.30)
    list(APPEND PAG_OPTIONS "-DCMAKE_CXX_FLAGS=\"-D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR=1\"")
endif ()
list(APPEND PAG_OPTIONS "-DCMAKE_PREFIX_PATH=\"${CMAKE_PREFIX_PATH}\"")
if (CMAKE_BUILD_TYPE MATCHES "Debug")
    list(APPEND PAG_OPTIONS "-d")
    string(TOLOWER ${CMAKE_BUILD_TYPE} TGFX_BUILD_TYPE)
endif ()
set(PAG_OUTPUT_DIR ${CMAKE_BINARY_DIR}/libpag)
set(PAG_OUTPUT_LIB ${PAG_OUTPUT_DIR}/${ARCH}/libpag${CMAKE_STATIC_LIBRARY_SUFFIX})
set(PAG_OUTPUT_MD5 ${PAG_OUTPUT_DIR}/.pag.${ARCH}.md5)
add_custom_target(pag
        COMMAND node ${CMAKE_SOURCE_DIR}/../build_pag ${PAG_OPTIONS} -p ${PLATFORM} -a ${ARCH} -o ${PAG_OUTPUT_DIR}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        BYPRODUCTS ${PAG_OUTPUT_LIB} ${PAG_OUTPUT_MD5}
        VERBATIM USES_TERMINAL)
list(APPEND libs ${PAG_OUTPUT_LIB})
list(APPEND PAG_DEPEND_TARGETS pag)

add_library(PAGExporter MODULE ${PAG_EXPORTER_SOURCE_FILES} ${PAG_EXPORTER_THIRD_PARTY_SOURCE_FILES} ${QT_RESOURCES})
if (PAG_DEPEND_TARGETS)
    add_dependencies(PAGExporter ${PAG_DEPEND_TARGETS})
endif ()

target_include_directories(PAGExporter SYSTEM PRIVATE ${PAG_EXPORTER_THIRD_PARTY_INCLUDES})
target_include_directories(PAGExporter PUBLIC ${PAG_EXPORTER_INCLUDES})
target_link_libraries(PAGExporter ${libs})

if (APPLE)
    target_link_libraries(PAGExporter "-framework AppKit" compression)
    set_target_properties(PAGExporter PROPERTIES
            BUNDLE TRUE
            BUNDLE_EXTENSION "plugin"
            MACOSX_BUNDLE_BUNDLE_NAME "PAGExporter"
            MACOSX_BUNDLE_BUNDLE_VERSION "1.0"
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.example.PAGExporter"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/resource/mac/PAGExporter.plugin-Info.plist"
            OUTPUT_NAME "PAGExporter"
    )

    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/PAGExporter.rsrc
            COMMAND Rez -o ${CMAKE_CURRENT_SOURCE_DIR}/PAGExporter.rsrc ${CMAKE_CURRENT_SOURCE_DIR}/PAGExporterPiPL.r
            -i ${AE_SDK_PATH}/Headers
            -i ${AE_SDK_PATH}/Resources
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/PAGExporterPiPL.r
            COMMENT "Generating PAGExporter.rsrc"
            VERBATIM
    )
    add_custom_target(
            GeneratePAGExporterRsrc
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/PAGExporter.rsrc
    )
    add_dependencies(PAGExporter GeneratePAGExporterRsrc)

    set(RSRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/PAGExporter.rsrc)
    set(RESOURCES_DIR "$<TARGET_BUNDLE_DIR:PAGExporter>/Contents/Resources")
    add_custom_command(
            TARGET PAGExporter POST_BUILD
            COMMAND /bin/sh -c "mkdir -p ${RESOURCES_DIR}"
            COMMAND /bin/sh -c "cp -r ${RSRC_FILE} ${RESOURCES_DIR}"
            COMMENT "Copying ${RSRC_FILE} to ${RESOURCES_DIR}"
            VERBATIM
    )

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_custom_command(
                TARGET PAGExporter POST_BUILD
                COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/mac/debugbuild.sh "$<TARGET_BUNDLE_DIR:PAGExporter>"
                COMMENT "Running debugbuild script..."
                VERBATIM
        )
    endif ()
elseif (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            SUFFIX ".aex"
    )
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rc
            COMMAND cl /I ${AE_SDK_PATH}/Headers /EP ${CMAKE_CURRENT_SOURCE_DIR}/PAGExporterPiPL.r > ${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rr
            COMMAND ${AE_SDK_PATH}/Resources/PiPLTool ${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rr ${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rrc
            COMMAND cl /D "MSWindows" /EP ${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rrc > ${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rc
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/PAGExporterPiPL.r
            COMMENT "Compiling the PiPL"
    )
    target_sources(${PROJECT_NAME} PRIVATE PAGExporterPiPL.rc)

    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        set(deploy_build_type "debug")
    elseif (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        set(deploy_build_type "release")
    endif ()

    get_target_property(qmake_executable Qt6::qmake IMPORTED_LOCATION)
    if (NOT qmake_executable)
        message(FATAL_ERROR "qmake executable not found")
    endif ()
    get_filename_component(qt_bin_dir "${qmake_executable}" DIRECTORY)
    find_program(windeployqt_executable windeployqt HINTS "${qt_bin_dir}")
    if (NOT windeployqt_executable)
        message(FATAL_ERROR "windeployqt.exe not found in ${qt_bin_dir}")
    elseif (windeployqt_executable)
        message(STATUS "Found windeployqt.exe at: ${windeployqt_executable}")
        add_custom_command(
                TARGET PAGExporter POST_BUILD
                COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/win/deployqt.bat ${windeployqt_executable} "$<TARGET_FILE:PAGExporter>" ${deploy_build_type}
                COMMENT "Running deploy script..."
                VERBATIM
        )
    endif ()
endif ()
