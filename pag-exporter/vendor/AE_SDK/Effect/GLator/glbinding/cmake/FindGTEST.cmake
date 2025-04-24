
# GTEST_FOUND
# GTEST_INCLUDE_DIR
# GTEST_LIBRARIES

find_path(GTEST_INCLUDE_DIR gtest/gtest.h
    $ENV{GTESTDIR}/include
    $ENV{GTEST_HOME}/include
    $ENV{GMOCKDIR}/gtest/include
    $ENV{GMOCK_HOME}/gtest/include
    $ENV{PROGRAMFILES}/GTEST/include
    /usr/include
    /usr/local/include
    /sw/include
    /opt/local/include
    DOC "The directory where gtest/gtest.h resides")

find_library(GTEST_LIBRARY
    NAMES gtest
    PATHS
    $ENV{GTESTDIR}/lib
    $ENV{GTESTDIR}/lib/.libs
    $ENV{GTEST_HOME}/lib
    $ENV{GTEST_HOME}/lib/.libs
    $ENV{GTESTDIR}
    $ENV{GTEST_HOME}
    $ENV{GMOCKDIR}/gtest
    $ENV{GMOCK_HOME}/gtest
    $ENV{GTESTDIR}/Release
    $ENV{GTEST_HOME}/Release
    $ENV{GMOCKDIR}/gtest/Release
    $ENV{GMOCK_HOME}/gtest/Release
    /usr/lib64
    /usr/local/lib64
    /sw/lib64
    /opt/local/lib64
    /usr/lib
    /usr/local/lib
    /sw/lib
    /opt/local/lib
    DOC "The GTEST library")

find_library(GTEST_LIBRARY_DEBUG
    NAMES gtestd
    PATHS
    $ENV{GTESTDIR}/lib
    $ENV{GTESTDIR}/lib/.libs
    $ENV{GTEST_HOME}/lib
    $ENV{GTEST_HOME}/lib/.libs
    $ENV{GTESTDIR}
    $ENV{GTEST_HOME}
    $ENV{GMOCKDIR}/gtest
    $ENV{GMOCK_HOME}/gtest
    $ENV{GTESTDIR}/Debug
    $ENV{GTEST_HOME}/Debug
    $ENV{GMOCKDIR}/gtest/Debug
    $ENV{GMOCK_HOME}/gtest/Debug
    /usr/lib64
    /usr/local/lib64
    /sw/lib64
    /opt/local/lib64
    /usr/lib
    /usr/local/lib
    /sw/lib
    /opt/local/lib
    DOC "The GTEST debug library")

if (GTEST_LIBRARY AND GTEST_LIBRARY_DEBUG)
    set(GTEST_LIBRARIES "optimized" ${GTEST_LIBRARY} "debug" ${GTEST_LIBRARY_DEBUG})
elseif (GTEST_LIBRARY)
    set(GTEST_LIBRARIES ${GTEST_LIBRARY})
elseif (GTEST_LIBRARY_DEBUG)
    set(GTEST_LIBRARIES ${GTEST_LIBRARY_DEBUG})
else ()
    set(GTEST_LIBRARIES "")
endif ()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(GTEST REQUIRED_VARS GTEST_INCLUDE_DIR GTEST_LIBRARIES)
mark_as_advanced(GTEST_INCLUDE_DIR GTEST_LIBRARIES)
