
# Group source files in folders (e.g. for MSVC solutions)
# Example: source_group_by_path("${CMAKE_CURRENT_SOURCE_DIR}/src" 
#   "\\\\.h$|\\\\.hpp$|\\\\.cpp$|\\\\.c$|\\\\.ui$|\\\\.qrc$" "Source Files" ${sources})

function(source_group_by_path PARENT_PATH REGEX GROUP)

    foreach (FILENAME ${ARGN})
        
        get_filename_component(FILEPATH "${FILENAME}" REALPATH)
        file(RELATIVE_PATH FILEPATH ${PARENT_PATH} ${FILEPATH})
        get_filename_component(FILEPATH "${FILEPATH}" DIRECTORY)

        string(REPLACE "/" "\\" FILEPATH "${FILEPATH}")

    source_group("${GROUP}\\${FILEPATH}" REGULAR_EXPRESSION "${REGEX}" FILES ${FILENAME})

    endforeach()

endfunction()


# Extract entries matching a given regex from a list

function(list_extract OUTPUT REGEX)

    foreach(FILENAME ${ARGN})
        if(${FILENAME} MATCHES "${REGEX}")
            list(APPEND ${OUTPUT} ${FILENAME})
        endif()
    endforeach()

endfunction()



function(install_qt COMP DEST)

    if(WIN32)

    foreach(target ${ARGN})
        get_target_property(qtrelease Qt5::${target} LOCATION_RELEASE)
        install(FILES ${qtrelease} DESTINATION ${DEST} CONFIGURATIONS Release COMPONENT ${COMP})
        get_target_property(qtdebug Qt5::${target} LOCATION_DEBUG)
        install(FILES ${qtdebug} DESTINATION ${DEST}  CONFIGURATIONS Debug COMPONENT ${COMP})
    endforeach() 

    get_target_property(qtrelease Qt5::Core LOCATION_RELEASE)
    get_filename_component(qtdir ${qtrelease} DIRECTORY)

    if(NOT INSTALL_ICU_VERSION)
        set(INSTALL_ICU_VERSION "" CACHE FILEPATH "ICU version, e.g., icudt52.dll is version '52'." FORCE)
    endif()

    install(FILES 
        ${qtdir}/icudt${INSTALL_ICU_VERSION}.dll
        ${qtdir}/icuin${INSTALL_ICU_VERSION}.dll
        ${qtdir}/icuuc${INSTALL_ICU_VERSION}.dll
        DESTINATION ${DEST}
        COMPONENT ${COMP})

    endif()

endfunction()



function(install_qt_platforms COMP DEST)

    if(WIN32)

    get_target_property(qtrelease Qt5::Core LOCATION_RELEASE)
    get_filename_component(qtrelease_dir ${qtrelease} DIRECTORY)

    # eventhoug it is the same directory, this script should not rely on it
    get_target_property(qtdebug Qt5::Core LOCATION_DEBUG)
    get_filename_component(qtdebug_dir ${qtdebug} DIRECTORY)

    foreach(target ${ARGN})
        install(FILES "${qtrelease_dir}/../plugins/platforms/${target}.dll" DESTINATION ${DEST}/plugins/platforms 
            CONFIGURATIONS Release COMPONENT ${COMP})
        install(FILES "${qtdebug_dir}/../plugins/platforms/${target}d.dll" DESTINATION ${DEST}/plugins/platforms 
            CONFIGURATIONS Debug COMPONENT ${COMP})
    endforeach()    

    install(FILES ${CMAKE_SOURCE_DIR}/packages/qt.conf DESTINATION ${DEST} COMPONENT ${COMP})

    endif()

endfunction()
