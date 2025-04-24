
# GLBINDING_FOUND

# GLBINDING_LIBRARIES
# GLBINDING_INCLUDE_DIRS

# GLBINDING_LIBRARY
# GLBINDING_LIBRARY_RELEASE
# GLBINDING_LIBRARY_DEBUG
# GLBINDING_INCLUDE_DIR

# GLBINDING_BINARIES        (win32 only)
# GLBINDING_BINARY_RELEASE  (win32 only)
# GLBINDING_BINARY_DEBUG    (win32 only)

macro (find LIB_NAME HEADER)

    set(HINT_PATHS ${ARGN})

    if (${LIB_NAME} STREQUAL "glbinding")
        set(LIB_NAME_UPPER GLBINDING)
        set(LIBNAME glbinding)
    else()
        string(TOUPPER GLBINDING_${LIB_NAME} LIB_NAME_UPPER)
        set(LIBNAME glbinding${LIB_NAME})
    endif()

    find_path(${LIB_NAME_UPPER}_INCLUDE_DIR ${HEADER}
        ${ENVGLBINDING_DIR}/include
        ${ENVGLBINDING_DIR}/source/${LIBNAME}/include
        ${GLBINDING_DIR}/include
        ${GLBINDING_DIR}/source/${LIBNAME}/include
        ${ENVPROGRAMFILES}/glbinding/include
        /usr/include
        /usr/local/include
        /sw/include
        /opt/local/include
        DOC "The directory where ${header} resides")

    find_library(${LIB_NAME_UPPER}_LIBRARY_RELEASE
        NAMES ${LIBNAME}
        PATHS ${HINT_PATHS}
        DOC "The ${LIB_NAME} library")
    find_library(${LIB_NAME_UPPER}_LIBRARY_DEBUG
        NAMES ${LIBNAME}d
        PATHS ${HINT_PATHS}
        DOC "The ${LIB_NAME} debug library")
    
    if(${LIB_NAME_UPPER}_LIBRARY_RELEASE AND ${LIB_NAME_UPPER}_LIBRARY_DEBUG)
        set(${LIB_NAME_UPPER}_LIBRARY "optimized" ${${LIB_NAME_UPPER}_LIBRARY_RELEASE} "debug" ${${LIB_NAME_UPPER}_LIBRARY_DEBUG})
    elseif(${LIB_NAME_UPPER}_LIBRARY_RELEASE)
        set(${LIB_NAME_UPPER}_LIBRARY ${${LIB_NAME_UPPER}_LIBRARY_RELEASE})
    elseif(${LIB_NAME_UPPER}_LIBRARY_DEBUG)
        set(${LIB_NAME_UPPER}_LIBRARY ${${LIB_NAME_UPPER}_LIBRARY_DEBUG})
    endif()

    list(APPEND GLBINDING_INCLUDE_DIRS ${${LIB_NAME_UPPER}_INCLUDE_DIR})
    list(APPEND GLBINDING_LIBRARIES ${${LIB_NAME_UPPER}_LIBRARY})

    # DEBUG
    # message("${LIB_NAME_UPPER}_INCLUDE_DIR     = ${${LIB_NAME_UPPER}_INCLUDE_DIR}")
    # message("${LIB_NAME_UPPER}_LIBRARY_RELEASE = ${${LIB_NAME_UPPER}_LIBRARY_RELEASE}")
    # message("${LIB_NAME_UPPER}_LIBRARY_DEBUG   = ${${LIB_NAME_UPPER}_LIBRARY_DEBUG}")
    # message("${LIB_NAME_UPPER}_LIBRARY         = ${${LIB_NAME_UPPER}_LIBRARY}")

endmacro()


if(CMAKE_CURRENT_LIST_FILE)
    get_filename_component(GLBINDING_DIR ${CMAKE_CURRENT_LIST_FILE} PATH)
endif()

file(TO_CMAKE_PATH "$ENV{PROGRAMFILES}" ENVPROGRAMFILES)
file(TO_CMAKE_PATH "$ENV{GLBINDING_DIR}" ENVGLBINDING_DIR)

set(GLBINDING_INCLUDE_DIRS "")
set(GLBINDING_LIBRARIES "")

set(LIB_PATHS   
    ${GLBINDING_DIR}/build
    ${GLBINDING_DIR}/build/Release
    ${GLBINDING_DIR}/build/Debug
    ${GLBINDING_DIR}/build-release
    ${GLBINDING_DIR}/build-debug
    ${ENVGLBINDING_DIR}/lib
    ${GLBINDING_DIR}/lib
    ${ENVPROGRAMFILES}/glbinding/lib
    /usr/lib
    /usr/local/lib
    /sw/lib
    /opt/local/lib
    /usr/lib64
    /usr/local/lib64
    /sw/lib64
    /opt/local/lib64
)


find(glbinding glbinding/glbinding_api.h ${LIB_PATHS})

if (GLBINDING_LIBRARY AND WIN32)
    set(GLBINDING_BINARIES "")

    find_file(GLBINDING_BINARY_RELEASE
        NAMES glbinding.dll
        PATHS
        ${GLBINDING_DIR}/bin
        ${GLBINDING_DIR}/build/Release
        ${GLBINDING_DIR}/build-release
        DOC "The glbinding binary")

    find_file(GLBINDING_BINARY_DEBUG
        NAMES glbindingd.dll
        PATHS
        ${GLBINDING_DIR}/bin
        ${GLBINDING_DIR}/build/Debug
        ${GLBINDING_DIR}/build-debug
        DOC "The glbinding debug binary")

    if(NOT GLBINDING_BINARY_RELEASE STREQUAL "GLBINDING_BINARY_RELEASE-NOTFOUND")
        list(APPEND GLBINDING_BINARIES ${GLBINDING_BINARY_RELEASE})
    endif()

    if(NOT GLBINDING_BINARY_DEBUG STREQUAL "GLBINDING_BINARY_DEBUG-NOTFOUND")
        list(APPEND GLBINDING_BINARIES ${GLBINDING_BINARY_DEBUG})
    endif()

    # DEBUG
    # message("${LIB_NAME_UPPER}_BINARIES         = ${${LIB_NAME_UPPER}_BINARIES}")

endif()

# DEBUG
# message("GLBINDING_INCLUDE_DIRS  = ${GLBINDING_INCLUDE_DIRS}")
# message("GLBINDING_LIBRARIES = ${GLBINDING_LIBRARIES}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GLBINDING DEFAULT_MSG GLBINDING_LIBRARIES GLBINDING_INCLUDE_DIRS)
mark_as_advanced(GLBINDING_FOUND)
