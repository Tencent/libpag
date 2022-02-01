cmake_minimum_required(VERSION 3.1)
project(PAG)

# Uncomment the next 4 lines to build libpag for Intel cpus on Apple Silicon devices.
#if (APPLE AND NOT IOS)
#    SET(CMAKE_SYSTEM_PROCESSOR x86_64)
#    SET(CMAKE_OSX_ARCHITECTURES x86_64)
#endif ()

#set(CMAKE_VERBOSE_MAKEFILE ON)
include(./vendor_tools/vendor.cmake)

# Options for building libpag
option(PAG_USE_PNG_DECODE "allow use of embedded PNG library" ON)
option(PAG_USE_PNG_ENCODE "allow use of embedded PNG library" ON)
option(PAG_USE_JPEG_DECODE "allow use of embedded JPEG library" ON)
option(PAG_USE_JPEG_ENCODE "allow use of embedded JPEG library" ON)
option(PAG_USE_WEBP_DECODE "allow use of embedded WEBP library" ON)
option(PAG_USE_WEBP_ENCODE "allow use of embedded WEBP library" ON)
option(PAG_USE_FREETYPE "allow use of embedded freetype library" ON)
option(PAG_USE_LIBAVC "allow use of embedded libavc as fallback video decoder" ON)
option(PAG_USE_OPENGL "allow use of OpenGL as GPU backend" ON)
option(PAG_USE_SWIFTSHADER "allow build with SwiftShader library" OFF)
option(PAG_USE_QT "allow build with QT frameworks" OFF)
option(PAG_USE_RTTR "enable RTTR support" OFF)
option(PAG_BUILD_SHARED "Build shared library" ON)
option(PAG_BUILD_TESTS "Build libpag tests" OFF)

if (IOS)
    set(PAG_USE_PNG_DECODE OFF CACHE BOOL "allow use of embedded PNG library" FORCE)
    set(PAG_USE_PNG_ENCODE OFF CACHE BOOL "allow use of embedded PNG library" FORCE)
    set(PAG_USE_JPEG_DECODE OFF CACHE BOOL "allow use of embedded JPEG library" FORCE)
    set(PAG_USE_JPEG_ENCODE OFF CACHE BOOL "allow use of embedded JPEG library" FORCE)
    set(PAG_USE_WEBP_ENCODE OFF CACHE BOOL "allow use of embedded WEBP library" FORCE)
    set(PAG_USE_FREETYPE OFF CACHE BOOL "allow use of embedded freetype library" FORCE)
elseif (ANDROID)
    set(PAG_USE_PNG_ENCODE OFF CACHE BOOL "allow use of embedded PNG library" FORCE)
    set(PAG_USE_JPEG_DECODE OFF CACHE BOOL "allow use of embedded JPEG library" FORCE)
    set(PAG_USE_JPEG_ENCODE OFF CACHE BOOL "allow use of embedded JPEG library" FORCE)
    set(PAG_USE_WEBP_DECODE OFF CACHE BOOL "allow use of embedded WEBP library" FORCE)
    set(PAG_USE_WEBP_ENCODE OFF CACHE BOOL "allow use of embedded WEBP library" FORCE)
    set(PAG_BUILD_SHARED ON CACHE BOOL "Build shared lib" FORCE)
elseif (WEB)
    set(PAG_USE_PNG_DECODE OFF CACHE BOOL "allow use of embedded PNG library" FORCE)
    set(PAG_USE_PNG_ENCODE OFF CACHE BOOL "allow use of embedded PNG library" FORCE)
    set(PAG_USE_JPEG_DECODE OFF CACHE BOOL "allow use of embedded JPEG library" FORCE)
    set(PAG_USE_JPEG_ENCODE OFF CACHE BOOL "allow use of embedded JPEG library" FORCE)
    set(PAG_USE_WEBP_ENCODE OFF CACHE BOOL "allow use of embedded WEBP library" FORCE)
    set(PAG_USE_FREETYPE OFF CACHE BOOL "allow use of embedded freetype library" FORCE)
    set(PAG_USE_LIBAVC OFF CACHE BOOL "allow use of embedded libavc as fallback video decoder" FORCE)
    set(PAG_BUILD_SHARED OFF CACHE BOOL "Build shared library" FORCE)
elseif (MACOS)
    # CLion project needs test targets.
    set(PAG_BUILD_TESTS ON CACHE BOOL "Build libpag tests" FORCE)
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
endif (MSVC)

# Sets flags
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_definitions(-DDEBUG)
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer -g -O1")
    #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=leak -g -O1")
    #    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread -fPIE -g -O1")
    #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=nullability -g -O1")
endif ()

# collects pag include directories.
set(PAG_INCLUDES ./ include src tgfx/include tgfx/src)

# collects pag source files.
file(GLOB_RECURSE PAG_FILES
        src/base/*.*
        src/codec/*.*
        src/rendering/*.*
        tgfx/src/base/*.*
        tgfx/src/core/*.*)
if (NOT WEB)
    file(GLOB PAG_VIDEO_FILES src/video/*.*)
    list(APPEND PAG_FILES ${PAG_VIDEO_FILES})
endif ()
file(GLOB PAG_COMMON_FILES
        src/platform/*.*
        tgfx/src/gpu/*.*
        tgfx/src/image/*.*
        tgfx/src/raster/*.*)
list(APPEND PAG_FILES ${PAG_COMMON_FILES})

if (PAG_USE_QT)
    set(PAG_USE_SWIFTSHADER OFF)
endif ()

if (PAG_USE_QT OR PAG_USE_SWIFTSHADER)
    set(PAG_USE_OPENGL ON)
else ()
    set(USE_NATIVE_PLATFORM ON)
endif ()

if (PAG_USE_FREETYPE)
    # Freetype needs libpng
    set(PAG_USE_PNG_DECODE ON)
endif ()

if (PAG_USE_PNG_DECODE)
    add_definitions(-DTGFX_USE_PNG_DECODE)
    set(PAG_USE_PNG ON)
endif ()

if (PAG_USE_PNG_ENCODE)
    add_definitions(-DTGFX_USE_PNG_ENCODE)
    set(PAG_USE_PNG ON)
endif ()

if (PAG_USE_PNG)
    set(PAG_USE_ZLIB ON)
endif ()

if (PAG_USE_JPEG_DECODE)
    add_definitions(-DTGFX_USE_JPEG_DECODE)
    set(PAG_USE_JPEG ON)
endif ()

if (PAG_USE_JPEG_ENCODE)
    add_definitions(-DTGFX_USE_JPEG_ENCODE)
    set(PAG_USE_JPEG ON)
endif ()

if (PAG_USE_WEBP_DECODE)
    add_definitions(-DTGFX_USE_WEBP_DECODE)
    set(PAG_USE_WEBP ON)
endif ()

if (PAG_USE_WEBP_ENCODE)
    add_definitions(-DTGFX_USE_WEBP_ENCODE)
    set(PAG_USE_WEBP ON)
endif ()

if (PAG_USE_OPENGL)
    file(GLOB GFX_PLATFORM_FILES tgfx/src/gpu/opengl/*.*)
    list(APPEND PAG_FILES ${GFX_PLATFORM_FILES})
endif ()

if (PAG_USE_FREETYPE)
    add_definitions(-DTGFX_USE_FREETYPE)
    list(APPEND PAG_STATIC_VENDORS freetype)
    file(GLOB_RECURSE GFX_PLATFORM_FILES tgfx/src/raster/freetype/*.*)
    list(APPEND PAG_FILES ${GFX_PLATFORM_FILES})
    list(APPEND PAG_INCLUDES third_party/freetype/include)
elseif (WEB)
    add_definitions(-DTGFX_BUILD_FOR_WEB)
    file(GLOB_RECURSE GFX_PLATFORM_FILES tgfx/src/raster/web/*.*)
    list(APPEND PAG_FILES ${GFX_PLATFORM_FILES})
elseif (APPLE)
    # Uses CoreGraphics instead.
    add_definitions(-DTGFX_USE_CORE_GRAPHICS)
    file(GLOB_RECURSE GFX_PLATFORM_FILES tgfx/src/raster/coregraphics/*.*)
    list(APPEND PAG_FILES ${GFX_PLATFORM_FILES})
endif ()

if (PAG_USE_JPEG)
    list(APPEND PAG_STATIC_VENDORS libjpeg-turbo)
    file(GLOB_RECURSE GFX_PLATFORM_FILES tgfx/src/image/jpeg/*.*)
    list(APPEND PAG_FILES ${GFX_PLATFORM_FILES})
    list(APPEND PAG_INCLUDES third_party/out/libjpeg-turbo/${INCLUDE_ENTRY})
endif ()

if (PAG_USE_WEBP)
    list(APPEND PAG_STATIC_VENDORS libwebp)
    file(GLOB_RECURSE GFX_PLATFORM_FILES tgfx/src/image/webp/*.*)
    list(APPEND PAG_FILES ${GFX_PLATFORM_FILES})
    list(APPEND PAG_INCLUDES third_party/libwebp/src)
endif ()

if (PAG_USE_PNG)
    list(APPEND PAG_STATIC_VENDORS libpng)
    file(GLOB_RECURSE GFX_PLATFORM_FILES tgfx/src/image/png/*.*)
    list(APPEND PAG_FILES ${GFX_PLATFORM_FILES})
    list(APPEND PAG_INCLUDES third_party/out/libpng/${INCLUDE_ENTRY})
endif ()

if (PAG_USE_ZLIB)
    if (NOT ANDROID)
        list(APPEND PAG_STATIC_VENDORS zlib)
    endif ()
endif ()

if (PAG_USE_LIBAVC)
    add_definitions(-DPAG_USE_LIBAVC)
    list(APPEND PAG_STATIC_VENDORS libavc)
    list(APPEND PAG_INCLUDES third_party/libavc/common third_party/libavc/decoder)
endif ()

list(APPEND SWIFTSHADER_INCLUDE_DIRS vendor/swiftshader/include)
file(GLOB SWIFTSHADER_LIBRARIES vendor/swiftshader/${LIBRARY_ENTRY}/*${CMAKE_SHARED_LIBRARY_SUFFIX})

if (PAG_USE_QT)
    # need to set the CMAKE_PREFIX_PATH to local QT installation path, for example :
    # set(CMAKE_PREFIX_PATH /Users/username/Qt5.13.0/5.13.0/clang_64/lib/cmake)
    find_package(Qt5OpenGL REQUIRED)
    find_package(Qt5Gui REQUIRED)
    find_package(Qt5Quick REQUIRED)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${Qt5OpenGL_LIBRARIES} ${Qt5Gui_LIBRARIES} ${Qt5Quick_LIBRARIES})
    list(APPEND PAG_PLATFORM_INCLUDES ${Qt5OpenGL_INCLUDE_DIRS} ${Qt5Gui_INCLUDE_DIRS} ${Qt5Quick_INCLUDE_DIRS})
    file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/qt/*.*)
    file(GLOB GL_PLATFORM_FILES tgfx/src/gpu/opengl/qt/*.*)
    list(APPEND PAG_PLATFORM_FILES ${GL_PLATFORM_FILES})
elseif (PAG_USE_SWIFTSHADER)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${SWIFTSHADER_LIBRARIES})
    list(APPEND PAG_PLATFORM_INCLUDES ${SWIFTSHADER_INCLUDE_DIRS})
    file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/swiftshader/*.*)
    file(GLOB GL_PLATFORM_FILES tgfx/src/gpu/opengl/egl/*.*)
    list(APPEND PAG_PLATFORM_FILES ${GL_PLATFORM_FILES})
endif ()

if (PAG_USE_RTTR)
    add_definitions(-DPAG_USE_RTTR)
    list(APPEND PAG_STATIC_VENDORS rttr)
    list(APPEND PAG_INCLUDES third_party/out/rttr/${INCLUDE_ENTRY})
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

list(APPEND PAG_STATIC_VENDORS pathkit)
list(APPEND PAG_INCLUDES third_party/pathkit)
list(APPEND PAG_STATIC_VENDORS skcms)
list(APPEND PAG_INCLUDES third_party/skcms)

if (WEB)
    add_definitions(-DPAG_BUILD_FOR_WEB)
    if (USE_NATIVE_PLATFORM)
        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/web/*.*)
        file(GLOB_RECURSE TGFX_PLATFORM_FILES tgfx/src/platform/web/*.*)
        list(APPEND PAG_PLATFORM_FILES ${TGFX_PLATFORM_FILES})
        file(GLOB GL_PLATFORM_FILES tgfx/src/gpu/opengl/webgl/*.*)
        list(APPEND PAG_PLATFORM_FILES ${GL_PLATFORM_FILES})
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

    if (PAG_USE_OPENGL)
        find_library(OpenGLES_LIBS OpenGLES REQUIRED)
        list(APPEND PAG_PLATFORM_SHARED_LIBS ${OpenGLES_LIBS})
        add_definitions(-DGL_SILENCE_DEPRECATION)
    else ()
        # Uses Metal instead.
        file(GLOB_RECURSE GFX_PLATFORM_FILES tgfx/src/gpu/metal/*.*)
        list(APPEND PAG_FILES ${GFX_PLATFORM_FILES})
        find_library(Metal_LIBS Metal REQUIRED)
        list(APPEND PAG_PLATFORM_SHARED_LIBS ${Metal_LIBS})
    endif ()

    if (USE_NATIVE_PLATFORM)
        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/ios/*.* src/platform/cocoa/*.*)
        file(GLOB_RECURSE TGFX_PLATFORM_FILES tgfx/src/platform/apple/*.*)
        list(APPEND PAG_PLATFORM_FILES ${TGFX_PLATFORM_FILES})
        file(GLOB GL_PLATFORM_FILES tgfx/src/gpu/opengl/eagl/*.*)
        list(APPEND PAG_PLATFORM_FILES ${GL_PLATFORM_FILES})
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

    if (PAG_USE_OPENGL)
        find_library(OpenGL_LIBS OpenGL REQUIRED)
        list(APPEND PAG_PLATFORM_SHARED_LIBS ${OpenGL_LIBS})
        add_definitions(-DGL_SILENCE_DEPRECATION)
    else ()
        # Uses Metal instead.
        file(GLOB_RECURSE GFX_PLATFORM_FILES tgfx/src/gpu/metal/*.*)
        list(APPEND PAG_FILES ${GFX_PLATFORM_FILES})
    endif ()

    if (USE_NATIVE_PLATFORM)
        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/mac/*.* src/platform/cocoa/*.*)
        file(GLOB_RECURSE TGFX_PLATFORM_FILES tgfx/src/platform/apple/*.*)
        list(APPEND PAG_PLATFORM_FILES ${TGFX_PLATFORM_FILES})
        file(GLOB GL_PLATFORM_FILES tgfx/src/gpu/opengl/cgl/*.*)
        list(APPEND PAG_PLATFORM_FILES ${GL_PLATFORM_FILES})
        find_include_dirs(MAC_PLATFORM_INCLUDES
                src/platform/mac/*.h
                src/platform/mac/private/*.h
                src/platform/cocoa/*.h
                src/platform/cocoa/private/*.h)
        list(APPEND PAG_PLATFORM_INCLUDES ${MAC_PLATFORM_INCLUDES})
    endif ()
elseif (ANDROID)

    # finds all required platform libraries.
    find_library(LOG_LIB log)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${LOG_LIB})
    find_library(GLESV2_LIB GLESv2)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${GLESV2_LIB})
    find_library(EGL_LIB EGL)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${EGL_LIB})
    find_library(ANDROID_LIB android)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${ANDROID_LIB})
    find_library(JNIGRAPHICS_LIB jnigraphics)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${JNIGRAPHICS_LIB})
    if (PAG_USE_ZLIB)
        find_library(Z_LIB z)
        list(APPEND PAG_PLATFORM_SHARED_LIBS ${Z_LIB})
    endif ()

    # optimizes the output size
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--gc-sections -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/android/libpag/export.def")
    add_compile_options(-ffunction-sections -fdata-sections -Os -fno-exceptions -fno-rtti)

    if (USE_NATIVE_PLATFORM)
        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/android/*.*)
        file(GLOB_RECURSE TGFX_PLATFORM_FILES tgfx/src/platform/android/*.*)
        list(APPEND PAG_PLATFORM_FILES ${TGFX_PLATFORM_FILES})
        file(GLOB GL_PLATFORM_FILES tgfx/src/gpu/opengl/egl/*.*)
        list(APPEND PAG_PLATFORM_FILES ${GL_PLATFORM_FILES})
    endif ()
elseif (WIN32)
    set(BUILD_USE_64BITS ON)
    add_definitions(-DNOMINMAX -D_USE_MATH_DEFINES)

    if (USE_NATIVE_PLATFORM)
        list(APPEND PAG_PLATFORM_INCLUDES vendor/angle/include)
        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/win/*.*)
        file(GLOB_RECURSE TGFX_PLATFORM_FILES tgfx/src/platform/mock/*.*)
        list(APPEND PAG_PLATFORM_FILES ${TGFX_PLATFORM_FILES})
        file(GLOB GL_PLATFORM_FILES tgfx/src/gpu/opengl/egl/*.*)
        list(APPEND PAG_PLATFORM_FILES ${GL_PLATFORM_FILES})
    endif ()
    file(GLOB ANGLE_LIBS vendor/angle/${PLATFORM}/${ARCH}/*${CMAKE_STATIC_LIBRARY_SUFFIX})
    list(APPEND PAG_PLATFORM_STATIC_LIBS ${ANGLE_LIBS})
    find_library(Bcrypt_LIB Bcrypt)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${Bcrypt_LIB})
    find_library(ws2_32_LIB ws2_32)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${ws2_32_LIB})
elseif (CMAKE_HOST_SYSTEM_NAME MATCHES "Linux")
    # finds all required platform libraries.
    find_package(Threads)
    list(APPEND PAG_PLATFORM_SHARED_LIBS ${CMAKE_THREAD_LIBS_INIT})
    list(APPEND PAG_PLATFORM_SHARED_LIBS dl)
    add_compile_options(-fPIC -pthread)
    if (USE_NATIVE_PLATFORM)
        file(GLOB_RECURSE PAG_PLATFORM_FILES src/platform/linux/*.*)
        file(GLOB_RECURSE TGFX_PLATFORM_FILES tgfx/src/platform/mock/*.*)
        list(APPEND PAG_PLATFORM_FILES ${TGFX_PLATFORM_FILES})
        file(GLOB GL_PLATFORM_FILES tgfx/src/gpu/opengl/egl/*.*)
        list(APPEND PAG_PLATFORM_FILES ${GL_PLATFORM_FILES})
    endif ()
endif ()

add_vendor_target(PAG STATIC_VENDORS ${PAG_STATIC_VENDORS} SHARED_VENDORS ${PAG_SHARED_VENDORS})
list(APPEND PAG_PLATFORM_STATIC_LIBS ${PAG_VENDOR_STATIC_LIBRARIES})
list(APPEND PAG_PLATFORM_SHARED_LIBS ${PAG_VENDOR_SHARED_LIBRARIES})

if (PAG_BUILD_SHARED)
    add_library(pag SHARED ${PAG_VENDOR_TARGET} ${PAG_FILES} ${PAG_PLATFORM_FILES})
    target_compile_definitions(pag PUBLIC PAG_DLL)
    target_include_directories(pag PUBLIC ${PAG_INCLUDES} ${PAG_PLATFORM_INCLUDES})
    target_link_libraries(pag ${PAG_PLATFORM_STATIC_LIBS} ${PAG_PLATFORM_SHARED_LIBS})
else ()
    add_library(pag STATIC ${PAG_VENDOR_TARGET} ${PAG_FILES} ${PAG_PLATFORM_FILES})
    merge_libraries_into(pag ${PAG_PLATFORM_STATIC_LIBS})
    target_include_directories(pag PUBLIC ${PAG_INCLUDES} ${PAG_PLATFORM_INCLUDES})
    target_link_libraries(pag)
endif ()

if (PAG_BUILD_TESTS)
    file(GLOB_RECURSE SWIFTSHADER_PLATFORM_FILES src/platform/swiftshader/*.*)
    file(GLOB_RECURSE TGFX_PLATFORM_FILES tgfx/src/platform/mock/*.*)
    list(APPEND SWIFTSHADER_PLATFORM_FILES ${TGFX_PLATFORM_FILES})
    file(GLOB GL_PLATFORM_FILES tgfx/src/gpu/opengl/egl/*.*)
    list(APPEND SWIFTSHADER_PLATFORM_FILES ${GL_PLATFORM_FILES})
    add_library(pag-ss STATIC ${PAG_VENDOR_TARGET} ${PAG_FILES} ${SWIFTSHADER_PLATFORM_FILES})
    merge_libraries_into(pag-ss ${PAG_PLATFORM_STATIC_LIBS})
    target_include_directories(pag-ss PUBLIC ${PAG_INCLUDES} ${SWIFTSHADER_INCLUDE_DIRS})
    target_link_libraries(pag-ss)

    list(APPEND TEST_STATIC_VENDORS googletest)
    if (NOT PAG_USE_SWIFTSHADER)
        list(APPEND TEST_PLATFORM_LIBS ${SWIFTSHADER_LIBRARIES})
    endif ()
    add_vendor_target(Test STATIC_VENDORS ${TEST_STATIC_VENDORS} SHARED_VENDORS ${TEST_SHARED_VENDORS})
    list(APPEND TEST_PLATFORM_LIBS ${Test_VENDOR_STATIC_LIBRARIES})
    list(APPEND TEST_PLATFORM_LIBS ${Test_VENDOR_SHARED_LIBRARIES})
    set(TEST_PLATFORM_SHARED_LIBS ${PAG_PLATFORM_SHARED_LIBS})
    list(FILTER TEST_PLATFORM_SHARED_LIBS EXCLUDE REGEX "OpenGL")
    list(APPEND TEST_PLATFORM_LIBS pag-ss ${TEST_PLATFORM_SHARED_LIBS})
    set(TEST_INCLUDES third_party/googletest/googletest third_party/googletest/googletest/include third_party/json/include)
    list(APPEND TEST_INCLUDES ${PAG_INCLUDES})

    file(GLOB PAG_TEST_FILES
            test/*.*
            test/framework/*.*
            test/framework/lzma/*.*
            test/framework/utils/*.*)

    file(GLOB PAG_SMOKE_TEST_FILES
            test/PAGSmokeTest.cpp
            test/TestUtils.cpp
            test/framework/*.cpp
            test/framework/lzma/*.*
            test/framework/utils/*.cpp)

    file(GLOB PAG_PERFORMANCE_TEST_FILES
            test/PAGPerformanceTest.cpp
            test/TestUtils.cpp
            test/framework/*.cpp
            test/framework/lzma/*.*
            test/framework/utils/*.cpp)

    file(GLOB FFAVC_LIB vendor/ffavc/${LIBRARY_ENTRY}/*${CMAKE_SHARED_LIBRARY_SUFFIX})
    list(APPEND TEST_PLATFORM_LIBS ${FFAVC_LIB})
    list(APPEND TEST_INCLUDES vendor/ffavc/include)

    add_executable(PAGUnitTest ${Test_VENDOR_TARGET} ${PAG_TEST_FILES})
    target_include_directories(PAGUnitTest PUBLIC ${TEST_INCLUDES})
    # skip the PAGCompareFrameTest test case in PAGUnitTest
    target_compile_definitions(PAGUnitTest PUBLIC SKIP_FRAME_COMPARE)
    target_link_libraries(PAGUnitTest ${TEST_PLATFORM_LIBS})
    set_target_properties(PAGUnitTest PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control")

    add_executable(PAGFullTest ${Test_VENDOR_TARGET} ${PAG_TEST_FILES})
    target_include_directories(PAGFullTest PUBLIC ${TEST_INCLUDES})
    target_link_libraries(PAGFullTest ${TEST_PLATFORM_LIBS})
    set_target_properties(PAGFullTest PROPERTIES COMPILE_FLAGS "${CMAKE_CXX_FLAGS} -fno-access-control")

    add_executable(PAGSmokeTest ${Test_VENDOR_TARGET} ${PAG_SMOKE_TEST_FILES})
    target_include_directories(PAGSmokeTest PUBLIC ${TEST_INCLUDES})
    target_link_libraries(PAGSmokeTest ${TEST_PLATFORM_LIBS})
    target_compile_definitions(PAGSmokeTest PUBLIC SMOKE_TEST SKIP_FRAME_COMPARE)

    add_executable(PAGPerformanceTest ${Test_VENDOR_TARGET} ${PAG_PERFORMANCE_TEST_FILES})
    target_include_directories(PAGPerformanceTest PUBLIC ${TEST_INCLUDES})
    target_link_libraries(PAGPerformanceTest ${TEST_PLATFORM_LIBS})
    target_compile_definitions(PAGPerformanceTest PUBLIC PERFORMANCE_TEST)
endif ()
