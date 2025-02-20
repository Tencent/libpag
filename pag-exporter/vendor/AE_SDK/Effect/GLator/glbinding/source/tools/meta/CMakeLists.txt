
set(target meta)
message(STATUS "Tool ${target}")


# External libraries

find_package(OpenGL REQUIRED)


# Includes

include_directories(
    ${OPENGL_INCLUDE_DIR}
)

include_directories(
    BEFORE
    ${CMAKE_SOURCE_DIR}/source/glbinding/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)


# Libraries

set(libs
    ${OPENGL_LIBRARIES}
    glbinding
)


# Compiler definitions

if (OPTION_BUILD_STATIC)
    add_definitions("-DGLBINDING_STATIC")
endif()

if (OPTION_STRINGS_BY_GL)
    add_definitions("-DSTRINGS_BY_GL")
endif()
if (OPTION_GL_BY_STRINGS)
    add_definitions("-DGL_BY_STRINGS")
endif()


# Sources

set(sources
    main.cpp
)


# Build executable

add_executable(${target} ${sources})

target_link_libraries(${target} ${libs})

target_compile_options(${target} PRIVATE ${DEFAULT_COMPILE_FLAGS})

set_target_properties(${target}
    PROPERTIES
    LINKER_LANGUAGE              CXX
    FOLDER                      "${IDE_FOLDER}"
    COMPILE_DEFINITIONS_DEBUG   "${DEFAULT_COMPILE_DEFS_DEBUG}"
    COMPILE_DEFINITIONS_RELEASE "${DEFAULT_COMPILE_DEFS_RELEASE}"
    LINK_FLAGS_DEBUG            "${DEFAULT_LINKER_FLAGS_DEBUG}"
    LINK_FLAGS_RELEASE          "${DEFAULT_LINKER_FLAGS_RELEASE}"
    DEBUG_POSTFIX               "d${DEBUG_POSTFIX}")


# Deployment

install(TARGETS ${target} COMPONENT tools
    RUNTIME DESTINATION ${INSTALL_TOOLS}
#   LIBRARY DESTINATION ${INSTALL_SHARED}
#   ARCHIVE DESTINATION ${INSTALL_LIB}
)
