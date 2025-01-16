cmake_minimum_required(VERSION 3.1)
project(PAG)

# Uncomment the next 4 lines to build libpag for Intel cpus on Apple Silicon devices.
#if (APPLE AND NOT IOS)
#    SET(CMAKE_SYSTEM_PROCESSOR x86_64)
#    SET(CMAKE_OSX_ARCHITECTURES x86_64)
#endif ()

# Uncomment the next line to enable debug mode for third-party libraries.
#set(VENDOR_DEBUG ON)
#set(CMAKE_VERBOSE_MAKEFILE ON)
include(./vendor_tools/vendor.cmake)

# Options for building libpag
option(PAG_USE_OPENGL "allow use of OpenGL as GPU backend" ON)
option(PAG_USE_SWIFTSHADER "allow build with SwiftShader library" OFF)
option(PAG_USE_QT "allow build with QT5 frameworks" OFF)
option(PAG_USE_QT6 "allow build with QT6 frameworks" OFF)
option(PAG_USE_RTTR "enable RTTR support" OFF)
option(PAG_USE_HARFBUZZ "enable HarfBuzz support" OFF)

if (NOT APPLE AND NOT WEB)
    option(PAG_USE_FREETYPE "Allow use of embedded freetype library" ON)
endif ()

if (IOS OR WEB)
    option(PAG_USE_WEBP_DECODE "Enable embedded WEBP decoding support" ON)
elseif (ANDROID)
    option(PAG_USE_FFAVC "allow use of embedded ffavc as fallback video decoder" ON)
else ()
    option(PAG_USE_PNG_DECODE "Enable embedded PNG decoding support" ON)
    option(PAG_USE_PNG_ENCODE "Enable embedded PNG encoding support" ON)
    option(PAG_USE_JPEG_DECODE "Enable embedded JPEG decoding support" ON)
    option(PAG_USE_JPEG_ENCODE "Enable embedded JPEG encoding support" ON)
    option(PAG_USE_WEBP_DECODE "Enable embedded WEBP decoding support" ON)
    option(PAG_USE_WEBP_ENCODE "Enable embedded WEBP encoding support" ON)
endif ()

if (NOT WEB)
    option(PAG_USE_LIBAVC "allow use of embedded libavc as fallback video decoder" ON)
    option(PAG_BUILD_SHARED "Build shared library" ON)
endif ()

if (MACOS)
    # CLion project needs test targets.
    option(PAG_BUILD_TESTS "Build libpag tests" ON)
endif ()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
cmake_policy(SET CMP0063 NEW)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_definitions(-Werror -Wall -Wextra -Weffc++ -pedantic -Werror=return-type)
endif ()

if (MSVC)
    add_compile_options("/utf-8")
    add_compile_options(/w44251 /w44275)
endif (MSVC)

# Sets flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g -O1")
endif ()

# collects pag include directories.
set(PAG_INCLUDES ./ include src)

# collects pag source files.
file(GLOB_RECURSE PAG_FILES
        src/base/*.*
        src/codec/*.*
        src/rendering/*.*)

file(GLOB PAG_COMMON_FILES src/platform/*.*)
list(APPEND PAG_FILES ${PAG_COMMON_FILES})

if (PAG_USE_QT OR PAG_USE_QT6)
    set(PAG_USE_SWIFTSHADER OFF)
endif ()

if (PAG_USE_QT OR PAG_USE_QT6 OR PAG_USE_SWIFTSHADER)
    set(PAG_USE_OPENGL ON)
else ()
    set(USE_NATIVE_PLATFORM ON)
endif ()

if (PAG_BUILD_TESTS)
    set(PAG_USE_FREETYPE ON)
    set(PAG_USE_HARFBUZZ ON)
endif ()

message("PAG_USE_LIBAVC: ${PAG_USE_LIBAVC}")
message("PAG_USE_RTTR: ${PAG_USE_RTTR}")
message("PAG_USE_HARFBUZZ: ${PAG_USE_HARFBUZZ}")
message("PAG_BUILD_SHARED: ${PAG_BUILD_SHARED}")
message("PAG_BUILD_TESTS: ${PAG_BUILD_TESTS}")

set(TGFX_USE_OPENGL ${PAG_USE_OPENGL})
set(TGFX_USE_QT ${PAG_USE_QT})
set(TGFX_USE_QT6 ${PAG_USE_QT6})
set(TGFX_USE_SWIFTSHADER ${PAG_USE_SWIFTSHADER})
set(TGFX_USE_FREETYPE ${PAG_USE_FREETYPE})
set(TGFX_USE_PNG_DECODE ${PAG_USE_PNG_DECODE})
set(TGFX_USE_PNG_ENCODE ${PAG_USE_PNG_ENCODE})
set(TGFX_USE_JPEG_DECODE ${PAG_USE_JPEG_DECODE})
set(TGFX_USE_JPEG_ENCODE ${PAG_USE_JPEG_ENCODE})
set(TGFX_USE_WEBP_DECODE ${PAG_USE_WEBP_DECODE})
set(TGFX_USE_WEBP_ENCODE ${PAG_USE_WEBP_ENCODE})

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
add_subdirectory(tgfx/ EXCLUDE_FROM_ALL)
list(APPEND PAG_INCLUDES tgfx/include)

if (PAG_USE_LIBAVC)
    add_definitions(-DPAG_USE_LIBAVC)
    list(APPEND PAG_STATIC_VENDORS libavc)
    list(APPEND PAG_INCLUDES third_party/libavc/common third_party/libavc/decoder)
endif ()

if (PAG_USE_QT)
    # need to set the CMAKE_PREFIX_PATH to local QT installation path, for example :
    # set(CMAKE_PREFIX_PATH /Users/username/Qt5.13.0/5.13.0/clang_64/lib/cmake)
    find_package(Qt5OpenGL REQUIRED)
    find_package(Qt5Gui REQUIRED)
    find_package(Qt5Quick REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${Qt5OpenGL_LIBRARIES} ${Qt5Gui_LIBRARIES} ${Qt5Quick_LIBRARIES})
    list(APPEND PAG_PLATFORM_INCLUDES ${Qt5OpenGL_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS} ${Qt5Quick_INCLUDE_DIRS})
    file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/qt/*.*)
    if (MACOS)
        file(GLOB CGL_PLATFORM_FILES src/platform/mac/private/HardwareDecoder.mm)
        list(APPEND PAG_PLATFORM_FILES ${CGL_PLATFORM_FILES})
    endif ()
elseif (PAG_USE_QT6)
    find_package(Qt6OpenGL REQUIRED)
    find_package(Qt6Gui REQUIRED)
    find_package(Qt6Quick REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${Qt6OpenGL_LIBRARIES} ${Qt6Gui_LIBRARIES} ${Qt6Quick_LIBRARIES})
    list(APPEND PAG_PLATFORM_INCLUDES ${Qt6OpenGL_INCLUDE_DIRS} ${Qt6Gui_INCLUDE_DIRS} ${Qt6Quick_INCLUDE_DIRS})
    file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/qt/*.*)
    if (MACOS)
        file(GLOB CGL_PLATFORM_FILES src/platform/mac/private/HardwareDecoder.mm)
        list(APPEND PAG_PLATFORM_FILES ${CGL_PLATFORM_FILES})
        add_definitions(-DCOREVIDEO_SILENCE_GL_DEPRECATION)
    endif ()
elseif (PAG_USE_SWIFTSHADER)
    file(GLOB SWIFTSHADER_LIBRARIES vendor/swiftshader/${LIBRARY_ENTRY}/*${CMAKE_SHARED_LIBRARY_SUFFIX})
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${SWIFTSHADER_LIBRARIES})
    list(APPEND PAG_PLATFORM_INCLUDES vendor/swiftshader/include)
    file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/swiftshader/*.*)
endif ()

if (PAG_USE_RTTR)
    add_definitions(-DPAG_USE_RTTR)
    list(APPEND PAG_STATIC_VENDORS rttr)
    list(APPEND PAG_INCLUDES third_party/out/rttr/${INCLUDE_ENTRY})
endif ()

if (PAG_USE_HARFBUZZ)
    add_definitions(-DPAG_USE_HARFBUZZ)
    list(APPEND PAG_STATIC_VENDORS harfbuzz)
    list(APPEND PAG_INCLUDES third_party/harfbuzz/src)
endif ()

function(find_include_dirs out)
    file(GLOB_RECURSE headers ${ARGN})
    foreach (path ${headers})
        get_filename_component(dir ${path} PATH)
        list(APPEND include_dirs ${dir})
    endforeach ()
    list(REMOVE_DUPLICATES include_dirs)
    set(${out} ${include_dirs} PARENT_SCOPE)
endfunction()

if (WEB)
    add_definitions(-DPAG_BUILD_FOR_WEB)
    if (USE_NATIVE_PLATFORM)
        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/web/*.*)
    endif ()
elseif (IOS)
    # finds all required platform libraries.
    find_library(UIKit_LIBS UIKit REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${UIKit_LIBS})
    find_library(Foundation_LIBS Foundation REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${Foundation_LIBS})
    find_library(QuartzCore_LIBS QuartzCore REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${QuartzCore_LIBS})
    find_library(CoreGraphics_LIBS CoreGraphics REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${CoreGraphics_LIBS})
    find_library(CoreText_LIBS CoreText REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${CoreText_LIBS})
    find_library(VideoToolbox_LIBS VideoToolbox REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${VideoToolbox_LIBS})
    find_library(CoreMedia_LIBS CoreMedia REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${CoreMedia_LIBS})
    find_library(CoreMedia_LIBS CoreMedia REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${CoreMedia_LIBS})
    find_library(ImageIO_LIBS ImageIO REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${ImageIO_LIBS})
    find_library(CORE_VIDEO CoreVideo)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${CORE_VIDEO})
    find_library(ICONV_LIBRARIES NAMES iconv libiconv libiconv-2 c)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${ICONV_LIBRARIES})

    if (USE_NATIVE_PLATFORM)
        if (PAG_USE_OPENGL)
            find_library(OpenGLES_LIBS OpenGLES REQUIRED)
            list(APPEND PAG_PLATFORM_SHARED_LIBS ${OpenGLES_LIBS})
            add_definitions(-DGL_SILENCE_DEPRECATION)
        else ()
            # Uses Metal instead.
            find_library(Metal_LIBS Metal REQUIRED)
            list(APPEND PAG_PLATFORM_SHARED_LIBS ${Metal_LIBS})
        endif ()

        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/ios/*.* src/platform/cocoa/*.*)
        find_include_dirs(IOS_PLATFORM_INCLUDES
                src/platform/ios/*.h
                src/platform/ios/private/*.h
                src/platform/cocoa/*.h
                src/platform/cocoa/private/*.h)
        list(APPEND PAG_PLATFORM_INCLUDES ${IOS_PLATFORM_INCLUDES})
    endif ()
elseif (MACOS)
    # finds all required platform libraries.
    find_library(APPLICATION_SERVICES_FRAMEWORK ApplicationServices REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${APPLICATION_SERVICES_FRAMEWORK})
    find_library(QUARTZ_CORE QuartzCore REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${QUARTZ_CORE})
    find_library(COCOA Cocoa REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${COCOA})
    find_library(FOUNDATION Foundation REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${FOUNDATION})
    find_library(ICONV_LIBRARIES NAMES iconv libiconv libiconv-2 c)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${ICONV_LIBRARIES})
    find_library(VIDEOTOOLBOX VideoToolbox)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${VIDEOTOOLBOX})
    find_library(CORE_MEDIA CoreMedia)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${CORE_MEDIA})

    if (USE_NATIVE_PLATFORM)
        if (PAG_USE_OPENGL)
            find_library(OpenGL_LIBS OpenGL REQUIRED)
            list(APPEND PAG_PLATFORM_SHARED_LIBS ${OpenGL_LIBS})
            add_definitions(-DGL_SILENCE_DEPRECATION)
        else ()
            # Uses Metal instead.
            find_library(Metal_LIBS Metal REQUIRED)
            list(APPEND PAG_PLATFORM_SHARED_LIBS ${Metal_LIBS})
        endif ()

        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/mac/*.* src/platform/cocoa/*.*)
        find_include_dirs(MAC_PLATFORM_INCLUDES
                src/platform/mac/*.h
                src/platform/mac/private/*.h
                src/platform/cocoa/*.h
                src/platform/cocoa/private/*.h)
        list(APPEND PAG_PLATFORM_INCLUDES ${MAC_PLATFORM_INCLUDES})
    endif ()
elseif (ANDROID)
    find_library(LOG_LIB log)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${LOG_LIB})
    find_library(ANDROID_LIB android)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${ANDROID_LIB})
    find_library(JNIGRAPHICS_LIB jnigraphics)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${JNIGRAPHICS_LIB})

    if (PAG_USE_FFAVC)
        add_definitions(-DPAG_USE_FFAVC)
        file(GLOB FFAVC_LIB android/libpag/libs/${ANDROID_ABI}/libffavc.so)
        list(APPEND PAG_PLATFORM_SHARED_LIBS ${FFAVC_LIB})
        list(APPEND PAG_PLATFORM_INCLUDES vendor/ffavc/include)
    endif ()

    # optimizes the output size
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/android/libpag/export.def")
    add_compile_options(-ffunction-sections -fdata-sections -Os -fno-exceptions -fno-rtti)

    if (USE_NATIVE_PLATFORM)
        find_library(GLESV2_LIB GLESv2)
        list(APPEND PAG_PLATFORM_SHARED_LIBS ${GLESV2_LIB})
        find_library(EGL_LIB EGL)
        list(APPEND PAG_PLATFORM_SHARED_LIBS ${EGL_LIB})
        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/android/*.*)
    endif ()
elseif (WIN32)
    set(BUILD_USE_64BITS ON)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES)

    if (USE_NATIVE_PLATFORM)
        list(APPEND PAG_PLATFORM_INCLUDES vendor/angle/include)
        file(GLOB ANGLE_LIBS vendor/angle/${PLATFORM}/${ARCH}/*${CMAKE_STATIC_LIBRARY_SUFFIX})
        list(APPEND PAG_PLATFORM_STATIC_LIBS ${ANGLE_LIBS})
        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/win/*.*)
    endif ()
    find_library(Bcrypt_LIB Bcrypt "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/x64")
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${Bcrypt_LIB})
    find_library(ws2_32_LIB ws2_32 "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.19041.0/um/x64")
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${ws2_32_LIB})
elseif (CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    # finds all required platform libraries.
    find_package(Threads)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${CMAKE_THREAD_LIBS_INIT})
    list(APPEND PAG_PLATFORM_SHARED_LIBS dl)
    add_compile_options(-fPIC -pthread)
    if (USE_NATIVE_PLATFORM)
        find_library(GLESV2_LIB GLESv2)
        list(APPEND PAG_PLATFORM_SHARED_LIBS ${GLESV2_LIB})
        find_library(EGL_LIB EGL)
        list(APPEND PAG_PLATFORM_SHARED_LIBS ${EGL_LIB})
        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/linux/*.*)
    endif ()
endif ()

if (MACOS OR IOS)
    find_library(COMPRESSION_LIBRARIES NAMES compression)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${COMPRESSION_LIBRARIES})
else ()
    list(APPEND PAG_INCLUDES third_party/lz4/lib)
    list(APPEND PAG_PLATFORM_FILES third_party/lz4/lib/lz4.c)
endif ()

add_vendor_target(PAG STATIC_VENDORS ${PAG_STATIC_VENDORS} SHARED_VENDORS ${PAG_SHARED_VENDORS})
list(APPEND PAG_PLATFORM_STATIC_LIBS ${PAG_VENDOR_STATIC_LIBRARIES})
list(APPEND PAG_PLATFORM_SHARED_LIBS ${PAG_VENDOR_SHARED_LIBRARIES})

if (PAG_BUILD_SHARED)
    add_library(pag SHARED ${PAG_VENDOR_TARGET} ${PAG_FILES} ${PAG_PLATFORM_FILES})
    target_compile_definitions(pag PUBLIC PAG_DLL)
    target_include_directories(pag PUBLIC ${PAG_INCLUDES} ${PAG_PLATFORM_INCLUDES})
    target_link_libraries(pag tgfx ${PAG_PLATFORM_STATIC_LIBS} ${PAG_PLATFORM_SHARED_LIBS})
else ()
    add_library(pag STATIC ${PAG_VENDOR_TARGET} ${PAG_FILES} ${PAG_PLATFORM_FILES} $<TARGET_OBJECTS:tgfx>)
    merge_libraries_into(pag ${PAG_PLATFORM_STATIC_LIBS} ${TGFX_PLATFORM_STATIC_LIBS})
    target_include_directories(pag PUBLIC ${PAG_INCLUDES} ${PAG_PLATFORM_INCLUDES})
    target_link_libraries(pag ${PAG_PLATFORM_STATIC_LIBS} ${TGFX_PLATFORM_STATIC_LIBS} ${PAG_PLATFORM_SHARED_LIBS})
endif ()

if (PAG_BUILD_TESTS)
    execute_process(COMMAND git rev-parse --short HEAD OUTPUT_VARIABLE HEAD_COMMIT)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/HEAD "${HEAD_COMMIT}")
    set(CacheVersionFile ${CMAKE_CURRENT_SOURCE_DIR}/test/baseline/.cache/version.json)
    if (EXISTS ${CacheVersionFile})
        execute_process(COMMAND git diff -G. --name-only origin/main:test/baseline/version.json ${CacheVersionFile} OUTPUT_VARIABLE HAS_DIFF)
    else ()
        set(HAS_DIFF ON)
    endif ()
    if (HAS_DIFF)
        message(WARNING "The local md5 cache is out of date! Please run the 'update_baseline.sh' to regenerate the cache.")
    endif ()

    add_library(pag-static STATIC ${PAG_VENDOR_TARGET} ${PAG_FILES} ${PAG_PLATFORM_FILES} $<TARGET_OBJECTS:tgfx>)
    merge_libraries_into(pag-static ${PAG_PLATFORM_STATIC_LIBS} ${TGFX_PLATFORM_STATIC_LIBS})
    target_include_directories(pag-static PUBLIC ${PAG_INCLUDES} ${PAG_PLATFORM_INCLUDES})
    target_link_libraries(pag-static tgfx ${PAG_PLATFORM_STATIC_LIBS} ${TGFX_PLATFORM_STATIC_LIBS} ${PAG_PLATFORM_SHARED_LIBS})

    list(APPEND TEST_STATIC_VENDORS googletest)
    add_vendor_target(Test STATIC_VENDORS ${TEST_STATIC_VENDORS} SHARED_VENDORS ${TEST_SHARED_VENDORS})
    list(APPEND TEST_PLATFORM_LIBS ${Test_VENDOR_STATIC_LIBRARIES})
    list(APPEND TEST_PLATFORM_LIBS ${Test_VENDOR_SHARED_LIBRARIES})
    list(APPEND TEST_PLATFORM_LIBS pag-static ${PAG_PLATFORM_SHARED_LIBS})
    set(TEST_INCLUDES third_party/googletest/googletest third_party/googletest/googletest/include third_party/json/include)
    list(APPEND TEST_INCLUDES ${PAG_INCLUDES})

    file(GLOB PAG_TEST_FILES
            test/*.*
            test/framework/*.*
            test/framework/utils/*.*)

    file(GLOB PAG_SMOKE_TEST_FILES
            test/PAGSmokeTest.cpp
            test/TestUtils.cpp
            test/framework/*.cpp
            test/framework/utils/*.cpp)

    file(GLOB PAG_PERFORMANCE_TEST_FILES
            test/PAGPerformanceTest.cpp
            test/TestUtils.cpp
            test/framework/*.cpp
            test/framework/utils/*.cpp)

    file(GLOB FFAVC_LIB vendor/ffavc/${PLATFORM}/${ARCH}/*${CMAKE_SHARED_LIBRARY_SUFFIX})
    list(APPEND TEST_PLATFORM_LIBS ${FFAVC_LIB})
    list(APPEND TEST_INCLUDES tgfx/src vendor/ffavc/include)

    # used to update the local md5 data for baseline testing.
    add_executable(PAGBaseline ${Test_VENDOR_TARGET} ${PAG_TEST_FILES})
    target_include_directories(PAGBaseline PUBLIC ${TEST_INCLUDES})
    target_compile_definitions(PAGBaseline PUBLIC UPDATE_BASELINE)
    target_link_libraries(PAGBaseline ${TEST_PLATFORM_LIBS})
    set_target_properties(PAGBaseline PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control")

    # used to generate baseline images to the out/ directory. each image has a "_base" suffix.
    add_executable(PAGGenerateImages ${Test_VENDOR_TARGET} ${PAG_TEST_FILES})
    target_include_directories(PAGGenerateImages PUBLIC ${TEST_INCLUDES})
    target_compile_definitions(PAGGenerateImages PUBLIC GENERATOR_BASELINE_IMAGES)
    target_link_libraries(PAGGenerateImages ${TEST_PLATFORM_LIBS})
    set_target_properties(PAGGenerateImages PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control")

    add_executable(PAGUnitTest ${Test_VENDOR_TARGET} ${PAG_TEST_FILES})
    target_include_directories(PAGUnitTest PUBLIC ${TEST_INCLUDES})
    target_compile_definitions(PAGUnitTest PUBLIC SKIP_FRAME_COMPARE)
    target_link_libraries(PAGUnitTest ${TEST_PLATFORM_LIBS})
    set_target_properties(PAGUnitTest PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control")

    add_executable(PAGFullTest ${Test_VENDOR_TARGET} ${PAG_TEST_FILES})
    target_include_directories(PAGFullTest PUBLIC ${TEST_INCLUDES})
    target_link_libraries(PAGFullTest ${TEST_PLATFORM_LIBS})
    set_target_properties(PAGFullTest PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control")

    add_executable(PAGPerformanceTest ${Test_VENDOR_TARGET} ${PAG_PERFORMANCE_TEST_FILES})
    target_include_directories(PAGPerformanceTest PUBLIC ${TEST_INCLUDES})
    target_compile_definitions(PAGPerformanceTest PUBLIC PERFORMANCE_TEST)
    target_link_libraries(PAGPerformanceTest ${TEST_PLATFORM_LIBS})
endif ()
