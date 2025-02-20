cmake_minimum_required(VERSION 3.14)
project(PAGExporter)

add_definitions(-DFFMPEG)
add_definitions(-DQT_NO_KEYWORDS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
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

file(GLOB_RECURSE UTIL_FILES ${ROOT}/src/utils/*.cpp)
list(APPEND SOURCE_FILES ${UTIL_FILES})

file(GLOB_RECURSE AESDK_FILES ${ROOT}/vendor/AE_SDK/Util/*.cpp)
list(APPEND SOURCE_FILES ${AESDK_FILES})

file(GLOB_RECURSE UIQT_FILES ${ROOT}/src/ui/qt/*.cpp)
list(APPEND SOURCE_FILES ${UIQT_FILES})

list(APPEND SOURCE_FILES
        ${ROOT}/src/aecmd/AECommand.cpp

        ${ROOT}/src/main.cpp)

list(APPEND PAGEXPORTER_INCLUDES
        ${ROOT}/vendor/AE_SDK/Headers
        ${ROOT}/vendor/AE_SDK/Headers/SP
        ${ROOT}/vendor/AE_SDK/Util
        ${ROOT}/vendor/AE_SDK/Resources
        ${ROOT}/src/util
        ${ROOT}/src/aecmd
        ${ROOT}/src/ui/qt)

if (APPLE)
    set(CMAKE_CXX_FLAGS "-stdlib=libc++")
    set_source_files_properties(${SOURCE_FILES} PROPERTIES
            COMPILE_FLAGS "-x objective-c++")
elseif (WIN32)
endif ()


add_library(PAGExporter MODULE ${SOURCE_FILES})
target_include_directories(PAGExporter PUBLIC ${PAGEXPORTER_INCLUDES})
target_link_libraries(PAGExporter Qt6::Widgets Qt6::OpenGL Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Multimedia Qt6::Network)
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
    # 添加自定义目标
    add_custom_target(
            GeneratePAGExporterRsrc
            DEPENDS ${ROOT}/PAGExporter.rsrc
    )
    add_dependencies(PAGExporter GeneratePAGExporterRsrc)

elseif (WIN32)
    # 设置目标扩展名
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
    # 将资源文件添加到项目中
    target_sources(${PROJECT_NAME} PRIVATE PAGExporterPiPL.rc)
endif ()