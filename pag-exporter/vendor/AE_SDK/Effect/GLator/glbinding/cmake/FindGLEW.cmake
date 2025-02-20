
# GLEW_FOUND
# GLEW_INCLUDE_DIR
# GLEW_LIBRARY

# GLEW_BINARY (win32 only)


find_path(GLEW_INCLUDE_DIR GL/glew.h
    $ENV{GLEWDIR}/include
    $ENV{GLEW_HOME}/include
    $ENV{PROGRAMFILES}/GLEW/include
    /usr/include
    /usr/local/include
    /sw/include
    /opt/local/include
    DOC "The directory where GL/glew.h resides")

if (X64)
    set(GLEW_BUILD_DIR Release/x64)
else()
    set(GLEW_BUILD_DIR Release/Win32)
endif()


find_library(GLEW_LIBRARY
    NAMES GLEW glew glew32 glew32s
    PATHS
    $ENV{GLEWDIR}/lib
    $ENV{GLEW_HOME}/lib
    $ENV{GLEWDIR}/lib/${GLEW_BUILD_DIR}
    $ENV{GLEW_HOME}/lib/${GLEW_BUILD_DIR}
    /usr/lib64
    /usr/local/lib64
    /sw/lib64
    /opt/local/lib64
    /usr/lib
    /usr/local/lib
    /sw/lib
    /opt/local/lib
    DOC "The GLEW library")

if(WIN32)

    find_file(GLEW_BINARY
        NAMES glew32.dll glew32s.dll
        PATHS
        $ENV{GLEWDIR}/bin
        $ENV{GLEW_HOME}/bin
        $ENV{GLEWDIR}/bin/${GLEW_BUILD_DIR}
        $ENV{GLEW_HOME}/bin/${GLEW_BUILD_DIR}
        DOC "The GLEW binary")

endif()
    
find_package_handle_standard_args(GLEW REQUIRED_VARS GLEW_INCLUDE_DIR GLEW_LIBRARY)
mark_as_advanced(GLEW_INCLUDE_DIR GLEW_LIBRARY)
