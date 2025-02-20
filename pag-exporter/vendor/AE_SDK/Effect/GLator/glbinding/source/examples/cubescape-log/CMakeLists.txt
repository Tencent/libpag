
set(target cubescape-log)


# External libraries

find_package(OpenGL REQUIRED)
find_package(GLFW QUIET)

if (NOT GLFW_FOUND)
	message("Example ${target} skipped: GLFW not found")
	return()
endif()

message(STATUS "Example ${target}")


# Includes

include_directories(
    ${OPENGL_INCLUDE_DIR}
    ${GLFW_INCLUDE_DIR}
)

include_directories(
    BEFORE
    ${CMAKE_SOURCE_DIR}/source/glbinding/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)


# Libraries

set(libs
    ${OPENGL_LIBRARIES}
    ${GLFW_LIBRARY}
    glbinding
)


# Compiler definitions

if (OPTION_BUILD_STATIC)
    add_definitions("-DGLBINDING_STATIC")
endif()


# Sources

set(cubscape_path ../cubescape)

set(sources
    main.cpp

    ${cubscape_path}/CubeScape.cpp
    ${cubscape_path}/CubeScape.h
    ${cubscape_path}/glutils.cpp
    ${cubscape_path}/glutils.h
    ${cubscape_path}/RawFile.cpp
    ${cubscape_path}/RawFile.h
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

install(TARGETS ${target} COMPONENT examples
    RUNTIME DESTINATION ${INSTALL_EXAMPLES}
#   LIBRARY DESTINATION ${INSTALL_SHARED}
#   ARCHIVE DESTINATION ${INSTALL_LIB}
)
