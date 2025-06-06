cmake_minimum_required(VERSION 3.14)
project(PAGExporter)

add_definitions(-DFFMPEG)
add_definitions(-DQT_NO_KEYWORDS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(LIBPAG_PATH ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(BUILD_ARCH "arm64")
# Compile different architectures based on parameters
if (APPLE)
    if (DEFINED BUILD_ARCH)
        if (${BUILD_ARCH} STREQUAL "arm64")
            set(CMAKE_SYSTEM_PROCESSOR "arm64")
            set(CMAKE_OSX_ARCHITECTURES "arm64")
            set(ARCH arm64)
        elseif (${BUILD_ARCH} STREQUAL "x86_64")
            set(CMAKE_SYSTEM_PROCESSOR "x86_64")
            set(CMAKE_OSX_ARCHITECTURES "x86_64")
            set(ARCH x64)
        endif ()
    endif ()
endif ()


set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/ui
)

if (NOT EXISTS ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg)
    if (APPLE)
        file(WRITE ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg
                "set(CMAKE_PREFIX_PATH  /usr/local/opt/qt@6/lib/cmake)")
    elseif (WIN32)
        file(WRITE ${PROJECT_SOURCE_DIR}/QTCMAKE.cfg
                "set(CMAKE_PREFIX_PATH  C:/Qt/6.8.1/msvc2022_64/lib/cmake)")
    endif ()
endif ()
include("./QTCMAKE.cfg")

find_package(Qt6 COMPONENTS Widgets Core Gui OpenGL Qml Quick Multimedia Network REQUIRED)
add_definitions(${Qt6Widgets_DEFINITIONS} ${QtQml_DEFINITIONS} ${${Qt6Quick_DEFINITIONS}})
qt6_add_resources(QT_RESOURCES assets/res.qrc)

file(GLOB UI_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui/*.ui")

file(GLOB_RECURSE SOURCE_FILES
        ${ROOT}/src/utils/*.cpp
        ${ROOT}/src/ui/qt/*.cpp
        ${ROOT}/src/exports/*.cpp
        ${ROOT}/src/cJSON/*.c
        ${ROOT}/src/VideoEncoder/*.cpp)

file(GLOB_RECURSE AESDK_FILES ${ROOT}/vendor/AE_SDK/Util/*.cpp)
list(APPEND SOURCE_FILES ${AESDK_FILES})
list(APPEND SOURCE_FILES
        ${ROOT}/src/aecmd/AECommand.cpp
        ${ROOT}/src/configparam/ConfigParam.cpp
        ${ROOT}/src/main.cpp)

list(APPEND PAGEXPORTER_INCLUDES
        ${ROOT}/src/utils
        ${ROOT}/src/aecmd
        ${ROOT}/src/ui/qt
        ${ROOT}/src/cJSON
        ${ROOT}/src/exports
        ${ROOT}/src/configparam
        ${ROOT}/src/VideoEncoder)

list(APPEND THIRDPARTY_INCLUDES
        ${ROOT}/vendor/AE_SDK/Headers
        ${ROOT}/vendor/AE_SDK/Headers/SP
        ${ROOT}/vendor/AE_SDK/Util
        ${ROOT}/vendor/AE_SDK/Resources
        ${ROOT}/third_party/json/include
        ${ROOT}/vendor/webp/include
        ${ROOT}/vendor/x264/include
#        ${ROOT}/vendor/ffmpeg/include
        ${ROOT}/vendor/ffaudio/include)

if (APPLE)
    set(CMAKE_CXX_FLAGS "-stdlib=libc++")
    set_source_files_properties(${SOURCE_FILES} PROPERTIES
            COMPILE_FLAGS "-x objective-c++")

    enable_language(OBJC)
    enable_language(OBJCXX)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version" FORCE)

    add_definitions(-DOS_MACOS)
    set(OS_ARCH darwin-x64)

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
#    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/pag/mac/libpag.a)
    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/webp/mac/libwebp.a)
    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/x264/mac/libx264.a)
#    file(GLOB ffmpeglibs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ffmpeg/mac/arm64/*.a)
#    list(APPEND libs ${ffmpeglibs})
    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ffaudio/mac/${ARCH}/libffaudio.dylib)
#    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ffaudio/mac/arm64/libffaudio.a)

elseif (WIN32)
    find_library(Bcrypt_LIB Bcrypt)
    list(APPEND libs ${Bcrypt_LIB})
    find_library(ws2_32_LIB ws2_32)
    list(APPEND libs ${ws2_32_LIB})
    find_library(DWrite_LIB DWrite)
    list(APPEND libs ${DWrite_LIB})
    file(GLOB ffmpeglibs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/ffmpeg/win/*.lib)
    list(APPEND libs ${ffmpeglibs})
    list(APPEND libs ${CMAKE_CURRENT_SOURCE_DIR}/vendor/pag/win/libpag.lib)
endif ()

set(PAG_USE_QT ON)
set(PAG_USE_RTTR OFF)
set(PAG_USE_LIBAVC ON)
set(PAG_BUILD_SHARED OFF)
set(PAG_BUILD_TESTS OFF)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../ ./libpag)

add_library(PAGExporter MODULE ${SOURCE_FILES} ${UI_FILES} ${QT_RESOURCES})
#target_compile_options(PAGExporter PUBLIC -Werror -Wall -Wextra -Weffc++ -Wconversion -pedantic -Werror=return-type -Wno-unused-command-line-argument)
target_include_directories(PAGExporter PUBLIC ${PAGEXPORTER_INCLUDES})
target_include_directories(PAGExporter SYSTEM PRIVATE ${THIRDPARTY_INCLUDES})
target_include_directories(PAGExporter PUBLIC ${LIBPAG_PATH}/include)
target_include_directories(PAGExporter PUBLIC ${LIBPAG_PATH}/src)
target_include_directories(PAGExporter PUBLIC ${LIBPAG_PATH}/third_party/tgfx/include)
message("libs:${libs}")
target_link_libraries(PAGExporter pag ${libs} Qt6::Widgets Qt6::OpenGL Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Multimedia Qt6::Network)
if (APPLE)
    set_target_properties(PAGExporter PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/plugin/${BUILD_ARCH}/)
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
    set_target_properties(PAGExporter PROPERTIES
            XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
            XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED "NO"
    )

    add_custom_command(
            OUTPUT ${ROOT}/PAGExporter.rsrc
            COMMAND Rez -o ${ROOT}/PAGExporter.rsrc ${ROOT}/PAGExporterPiPL.r
            -i ${ROOT}/vendor/AE_SDK/Headers
            -i ${ROOT}/vendor/AE_SDK/Resources
            DEPENDS ${ROOT}/PAGExporterPiPL.r
            COMMENT "Generating PAGExporter.rsrc"
            VERBATIM
    )
    add_custom_target(
            GeneratePAGExporterRsrc
            DEPENDS ${ROOT}/PAGExporter.rsrc
    )
    add_dependencies(PAGExporter GeneratePAGExporterRsrc)

    set(RSRC_FILE ${ROOT}/PAGExporter.rsrc)
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
                COMMAND ${ROOT}/scripts/mac/debugbuild.sh "$<TARGET_BUNDLE_DIR:PAGExporter>"  # 调用脚本并传递参数
                COMMENT "Running deugbuild script..."
                VERBATIM
        )
    endif ()
elseif (WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
            SUFFIX ".aex"
    )
    add_custom_command(
            OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rc
            COMMAND cl /I "${CMAKE_CURRENT_SOURCE_DIR}/vendor/AE_SDK/Headers" /EP "${CMAKE_CURRENT_SOURCE_DIR}/PAGExporterPiPL.r" > "${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rr"
            COMMAND "${CMAKE_CURRENT_SOURCE_DIR}/vendor/AE_SDK/Resources/PiPLTool" "${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rr" "${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rrc"
            COMMAND cl /D "MSWindows" /EP "${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rrc" > "${CMAKE_CURRENT_BINARY_DIR}/PAGExporterPiPL.rc"
            DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/PAGExporterPiPL.r"
            COMMENT "Compiling the PiPL"
    )
    target_sources(${PROJECT_NAME} PRIVATE PAGExporterPiPL.rc)

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
                COMMAND ${ROOT}/scripts/win/deployqt.bat "${windeployqt_executable}" "$<TARGET_FILE:PAGExporter>"  # 调用脚本并传递参数
                COMMENT "Running deploy script..."
                VERBATIM
        )

    endif ()

endif ()
