
find_program(PYTHON "python")

set(update_target update)
set(generate_target generate)

if (PYTHON)

    add_custom_target(${update_target})
    add_custom_target(${generate_target})

    add_custom_command (
        TARGET              ${update_target}
        PRE_BUILD
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND             ${PYTHON} scripts/update.py -f gl.xml -r gl.revision
        COMMENT             "Updating gl.xml from public SVN."
    )
    
    add_custom_command (
        TARGET              ${generate_target}
        PRE_BUILD
        WORKING_DIRECTORY   ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND             ${PYTHON} scripts/generate.py -s gl.xml -p patch.xml -d ../source/glbinding -r gl.revision
        COMMENT             "Generating OpenGL binding code."
    )

    # Declare target
    
    set_target_properties(${update_target} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
    set_target_properties(${generate_target} PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)

else()
    message("update target code skipped: python not found")
    message("generate target code skipped: python not found")
endif()
