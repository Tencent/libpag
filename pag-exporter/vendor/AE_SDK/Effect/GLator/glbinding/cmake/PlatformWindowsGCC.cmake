message(STATUS "Configuring for platform Windows/GCC.")

# support for C++11 etc.

execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
    OUTPUT_VARIABLE GCC_VERSION)

if(GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++11")
elseif(GCC_VERSION VERSION_EQUAL 4.6)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++0x")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()


set(MINWG_COMPILE_DEFS
    WIN32                       # Windows system
    UNICODE                     # Use unicode
    _UNICODE                    # Use unicode
    _CRT_SECURE_NO_DEPRECATE    # Disable CRT deprecation warnings
    PIC                         # Position-independent code
    _REENTRANT                  # Reentrant code
)
set(DEFAULT_COMPILE_DEFS_DEBUG
    ${MINWG_COMPILE_DEFS}
    _DEBUG                      # Debug build
)
set(DEFAULT_COMPILE_DEFS_RELEASE
    ${MINWG_COMPILE_DEFS}
    NDEBUG                      # Release build
)

if (OPTION_ERRORS_AS_EXCEPTION)
    set(EXCEPTION_FLAG "-fexceptions")
else()
    set(EXCEPTION_FLAG "-fno-exceptions")
endif()

set(MINGW_COMPILE_FLAGS
      
      ${EXCEPTION_FLAG}
      -pthread      # -> use pthread library
    # -no-rtti      # -> disable c++ rtti
      -pipe         # -> use pipes
      -Wall         # -> 
      -Wextra       # -> 
      -Werror       # ->
    # -fPIC         # -> use position independent code
      -Wreturn-type 
      -Wfloat-equal
      -Wcast-align 
      -Wconversion 
    # -Werror=return-type -> missing returns in functions and methods are handled as errors which stops the compilation
      -Wcast-align  # ->
    # -Wshadow      # -> e.g. when a parameter is named like a member, too many warnings, disabled for now
)

set(DEFAULT_COMPILE_FLAGS
    ${MINGW_COMPILE_FLAGS}
    $<$<CONFIG:Debug>:   
    >
    $<$<CONFIG:Release>: 
    >
)

set(MINGW_LINKER_FLAGS "-pthread")

set(DEFAULT_LINKER_FLAGS_RELEASE ${MINGW_LINKER_FLAGS})
set(DEFAULT_LINKER_FLAGS_DEBUG ${MINGW_LINKER_FLAGS})
set(DEFAULT_LINKER_FLAGS ${MINGW_LINKER_FLAGS})

# Add platform specific libraries for linking
set(EXTRA_LIBS "")
