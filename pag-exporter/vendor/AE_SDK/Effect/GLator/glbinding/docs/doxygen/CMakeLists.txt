
# Path to doxygen input
set(doxyfile_in doxyfile.in)

# Set project variables
set(doxyfile      ${CMAKE_CURRENT_BINARY_DIR}/doxyfile)
set(doxyfile_hhp  ${CMAKE_CURRENT_BINARY_DIR}/html/index.hhp)

# Check if doxygen is available
find_package(Doxygen)
if(DOXYGEN_FOUND)

    # Get filename and path of doxyfile
    get_filename_component(name ${doxyfile_in} NAME)
    get_filename_component(path ${doxyfile_in} PATH)
    if(NOT path)
        set(path ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    # Configure doxyfile (if it is a real doxyfile already, it should simply copy the file)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    configure_file(${doxyfile_in} ${doxyfile})

    # Invoke doxygen
    add_custom_command(
        OUTPUT              ${doxyfile_hhp}
        DEPENDS             ${doxyfile}
        WORKING_DIRECTORY   ${path}
        COMMAND             ${CMAKE_COMMAND} -E copy_directory ${path} ${CMAKE_CURRENT_BINARY_DIR}/html # ToDO, configure doxygen to use source as is
        COMMAND             ${DOXYGEN} \"${doxyfile}\"
        COMMENT             "Creating doxygen documentation for glbinding."
    )

    # Declare target
    add_custom_target(docs-doxygen DEPENDS ${doxyfile_hhp})
    set_target_properties(docs-doxygen PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)
    add_dependencies(docs docs-doxygen)

else()

    message("generate target documentation skipped: doxygen not found")

endif()
