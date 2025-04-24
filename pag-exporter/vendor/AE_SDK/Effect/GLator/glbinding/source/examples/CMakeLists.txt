
if(OPTION_BUILD_EXAMPLES)
    add_subdirectory("callbacks")
    add_subdirectory("comparison")
    add_subdirectory("cubescape")
    add_subdirectory("cubescape-log")
    add_subdirectory("cubescape-qt")

    install(FILES ${GLFW_BINARY} DESTINATION ${INSTALL_EXAMPLES} COMPONENT examples)
endif()
