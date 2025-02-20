
if(OPTION_BUILD_TOOLS)
    add_subdirectory("contexts")
    add_subdirectory("functions")
    add_subdirectory("meta")
    add_subdirectory("queries")

    install(FILES ${GLFW_BINARY} DESTINATION ${INSTALL_TOOLS} COMPONENT tools)
endif()
