
#message(STATUS "XCODE VERSION :: ${XCODE_VERSION}")

# Check compiler and version
if ("${XCODE_VERSION}" VERSION_LESS "5")

    if ("${XCODE_VERSION}" VERSION_GREATER "0") 
        message(FATAL_ERROR "Insufficient XCode Version (upgrade to XCode 5.x or higher)!")
    else ()
        message (STATUS "Configuring for platform MacOS without XCode, using plain make.")
    endif ()

else ()

    message(STATUS "Configuring for platform MacOS with XCode Version 5+.")

endif()


# removed according to http://comments.gmane.org/gmane.editors.lyx.cvs/38306
#set(CMAKE_XCODE_ATTRIBUTE_GCC_VERSION "com.apple.compilers.llvm.clang.1_0")

set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LANGUAGE_STANDARD "c++11")
set(CMAKE_XCODE_ATTRIBUTE_CLANG_CXX_LIBRARY "libc++")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++ -g -Wall")


set(MACOS_COMPILE_DEFS)
set(DEFAULT_COMPILE_DEFS_DEBUG   ${MACOS_COMPILE_DEFS} _DEBUG) # Debug build
set(DEFAULT_COMPILE_DEFS_RELEASE ${MACOS_COMPILE_DEFS} NDEBUG) # Release build

set(MACOS_LINKER_FLAGS "-framework Cocoa -framework OpenGL -framework IOKit -framework CoreVideo")
set(DEFAULT_LINKER_FLAGS_RELEASE ${MACOS_LINKER_FLAGS})
set(DEFAULT_LINKER_FLAGS_DEBUG ${MACOS_LINKER_FLAGS})

set(MACOS_COMPILE_FLAGS)
set(DEFAULT_COMPILE_FLAGS ${MACOS_COMPILE_FLAGS})

set(CMAKE_MACOSX_RPATH TRUE)
