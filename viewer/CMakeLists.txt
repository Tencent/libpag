cmake_minimum_required(VERSION 3.13)
project(PAGViewer)
set(APP_NAME PAGViewer)

include(tools/vendor_tools/vendor.cmake)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON CACHE INTERNAL "Enable Compile Commands Globally")

message(STATUS "Build OS: ${CMAKE_SYSTEM_NAME}")
if (WIN32)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MDd")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MD")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /OPT:REF /OPT:ICF")
    add_compile_options(/WX)
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/Zi>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/MP>")
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>")
elseif (APPLE)
    set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X Deployment Version" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-dead_strip")
    add_compile_options(-Werror)
else()
    message(STATUS "Unsupported OS: ${CMAKE_SYSTEM_NAME}")
endif()

if (CMAKE_PREFIX_PATH MATCHES "")
    if (EXISTS ${CMAKE_SOURCE_DIR}/QTCMAKE.cfg)
        include(${CMAKE_SOURCE_DIR}/QTCMAKE.cfg)
    else ()
        if(WIN32)
            set(QT_DIR "D:/application/qt6/6.8.1/msvc2022_64/lib/cmake" ${QT_DIR})
        elseif(APPLE)
            set(QT_DIR "/usr/local/opt/qt@6/lib/cmake" ${QT_DIR})
        endif()

        set(CMAKE_PREFIX_PATH ${QT_DIR} ${CMAKE_PREFIX_PATH})
    endif ()
endif ()
message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
if (NOT EXISTS ${CMAKE_SOURCE_DIR}/QTCMAKE.cfg)
    file(WRITE ${CMAKE_SOURCE_DIR}/QTCMAKE.cfg "set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})")
endif ()

message(STATUS "Generate Version File")
set(MajorVersion $ENV{BK_CI_MAJOR_VERSION} 2)
set(MinorVersion $ENV{BK_CI_MINOR_VERSION} 1)
set(FixVersion $ENV{BK_CI_FIX_VERSION} 93)
set(BuildNumber $ENV{BK_CI_BUILD_NO} 0)
if ($ENV{isBetaVersion} MATCHES "false")
    set(UpdateChannel "stable")    
else ()
    set(UpdateChannel "beta")
endif()
configure_file(${CMAKE_SOURCE_DIR}/tools/version_build_tools/version.h.in
    ${CMAKE_SOURCE_DIR}/src/common/version.h
    @ONLY
)
string(TIMESTAMP BuildTime "%Y-%m-%d %H:%M:%S")
message(STATUS "Build TIme: ${BuildTime}")
message(STATUS "Build Version: ${MajorVersion}.${MinorVersion}.${FixVersion}.${BuildNumber}")
message(STATUS "Update Channel: ${UpdateChannel}")

file(GLOB_RECURSE PAG_SRCS
    ${CMAKE_SOURCE_DIR}/src/common/*.*
)

file(GLOB_RECURSE PAG_HEADERS
    ${CMAKE_SOURCE_DIR}/src/*/*.h
    ${CMAKE_SOURCE_DIR}/src/*/*.hpp
)
list(APPEND PAG_INCLUDES ${CMAKE_SOURCE_DIR}/src)

if (WIN32)
    execute_process(COMMAND cmd /c "${CMAKE_SOURCE_DIR}/tools/build_tools/libpag/create_symbol_link.bat" "${CMAKE_SOURCE_DIR}/third_party/libpag" "${CMAKE_SOURCE_DIR}/../")
elseif (APPLE)
    execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/../ ${CMAKE_SOURCE_DIR}/third_party/libpag)
endif()

add_executable(${APP_NAME} ${PAG_SRCS})
target_include_directories(${APP_NAME} PRIVATE ${PAG_INCLUDES})

set(QT_MODULES
    Qml
    Gui
    Core
    Quick
    OpenGL
    Widgets
    Network
    Multimedia
    Core5Compat
    QuickControls2
)
foreach (QT_MODULE ${QT_MODULES})
    find_package(Qt6${QT_MODULE} REQUIRED)
    message(STATUS "Found Qt6${QT_MODULE} at ${Qt6${QT_MODULE}_LIBRARIES}")
    target_link_libraries(${APP_NAME} PRIVATE Qt6::${QT_MODULE})
    target_include_directories(${APP_NAME} PRIVATE ${Qt6${QT_MODULE}_INCLUDE_DIRS})
endforeach ()

list(APPEND VIEWER_VENDORS sodium)
list(APPEND VIEWER_VENDORS libpag)
list(APPEND VIEWER_VENDORS cjson)

if (WIN32)
    set_target_properties(${APP_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

    set(SYSTEM_LIBRARIES
        bcrypt
        ws2_32
        opengl32
    )

    foreach (LIBRARY ${SYSTEM_LIBRARIES})
        find_library(${LIBRARY}_LIBRARY NAMES ${LIBRARY} REQUIRED)
        if (${LIBRARY}_LIBRARY)
            message(STATUS "Found library ${LIBRARY} at ${${LIBRARY}_LIBRARY}")
            target_link_libraries(${APP_NAME} PRIVATE ${${LIBRARY}_LIBRARY})
        endif ()
    endforeach ()

    list(APPEND VIEWER_VENDORS winsparkle)
    add_vendor_target(viewer-vendor STATIC_VENDORS ${VIEWER_VENDORS} CONFIG_DIR ${CMAKE_SOURCE_DIR})
    add_dependencies(${APP_NAME} viewer-vendor)

    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../src)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/out/rttr/mac/include)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/tgfx/include)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/include)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/include)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/winsparkle/include)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/cjson/include)

    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/lib/${CMAKE_BUILD_TYPE}/libsodium.lib)
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/cjson/lib/${CMAKE_BUILD_TYPE}/libcjson.lib)
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libpag.lib)
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libpag-vendor.lib)
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libtgfx-vendor.lib)
    if (CMAKE_BUILD_TYPE MATCHES "Release")
        target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/winsparkle/lib/${CMAKE_BUILD_TYPE}/WinSparkle.lib)
    endif ()
elseif (APPLE)
    set_target_properties(${APP_NAME} PROPERTIES MACOSX_BUNDLE TRUE)

    set(SYSTEM_LIBRARYS
        xml2
        AGL
        Cocoa
        IOKit
        OpenGL
        CoreMedia
        CoreVideo
        QuartzCore
        Foundation
        compression
        VideoToolbox
        ApplicationServices
    )

    foreach (LIBRARY ${SYSTEM_LIBRARYS})
        find_library(${LIBRARY}_LIBRARY NAMES ${LIBRARY} REQUIRED)
        message(STATUS "Found library ${LIBRARY} at ${${LIBRARY}_LIBRARY}")
        target_link_libraries(${APP_NAME} PRIVATE ${${LIBRARY}_LIBRARY})
    endforeach ()

    set(SYSTEM_LIBRARIES_OPTIONAL
        c
        iconv
        iconv-2
    )
    foreach (LIBRARY ${SYSTEM_LIBRARIES_OPTIONAL})
        find_library(${LIBRARY}_LIBRARY NAMES ${LIBRARY})
        if (${LIBRARY}_LIBRARY)
            message(STATUS "Found library ${LIBRARY} at ${${LIBRARY}_LIBRARY}")
            target_link_libraries(${APP_NAME} PRIVATE ${${LIBRARY}_LIBRARY})
        else ()
            message(STATUS "Library ${LIBRARY} not found")
        endif ()
    endforeach ()

    list(APPEND VIEWER_VENDORS log4qt)
    list(APPEND VIEWER_VENDORS sparkle)
    add_vendor_target(viewer-vendor STATIC_VENDORS ${VIEWER_VENDORS} CONFIG_DIR ${CMAKE_SOURCE_DIR})
    add_dependencies(${APP_NAME} viewer-vendor)

    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../src)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/out/rttr/mac/include)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/tgfx/include)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/include)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/include)
    target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/cjson/include)

    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/lib/${CMAKE_BUILD_TYPE}/libsodium.a)
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/log4qt/lib/${CMAKE_BUILD_TYPE}/liblog4qt.a)
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/cjson/lib/${CMAKE_BUILD_TYPE}/libcjson.a)
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libpag.a)
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libpag-vendor.a)
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libtgfx-vendor.a)
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sparkle/lib/Release/Sparkle.framework)
endif()
