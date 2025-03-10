function(pag_add_qt6_library)
    cmake_parse_arguments(ARG "" "TARGET;REQUIRED" "MODULES" ${ARGN})

    if (${ARG_REQUIRED})
        foreach(MODULE ${ARG_MODULES})
            find_package(Qt6${MODULE} REQUIRED)
            message(STATUS "Found Qt6${MODULE} at ${Qt6${MODULE}_LIBRARIES}")
            target_link_libraries(${ARG_TARGET} PRIVATE Qt6::${MODULE})
            target_include_directories(${ARG_TARGET} PRIVATE ${Qt6${MODULE}_INCLUDE_DIRS})
        endforeach()
    else ()
        foreach(MODULE ${ARG_MODULES})
            find_package(Qt6${MODULE})
            if (Qt6${MODULE}_FOUND)
                message(STATUS "Found Qt6${MODULE} at ${Qt6${MODULE}_LIBRARIES}")
                target_link_libraries(${ARG_TARGET} PRIVATE Qt6::${MODULE})
                target_include_directories(${ARG_TARGET} PRIVATE ${Qt6${MODULE}_INCLUDE_DIRS})
            else ()
                message(STATUS "Do not Found Qt6${MODULE}, skipping")
            endif ()
        endforeach()
    endif ()
endfunction()

function(pag_add_framework)
    cmake_parse_arguments(ARG "" "TARGET;REQUIRED" "FRAMEWORKS;PATHS" ${ARGN})

    if (${ARG_REQUIRED})
        foreach(FRAMEWORK ${ARG_FRAMEWORKS})
            if(ARG_PATHS)
                find_library(${FRAMEWORK}_LIBRARY 
                    NAMES ${FRAMEWORK} 
                    PATHS ${ARG_PATHS}
                    REQUIRED
                    NO_DEFAULT_PATH
                )
                if(NOT ${FRAMEWORK}_LIBRARY)
                    find_library(${FRAMEWORK}_LIBRARY 
                        NAMES ${FRAMEWORK}
                        REQUIRED
                    )
                endif()
            else()
                find_library(${FRAMEWORK}_LIBRARY ${FRAMEWORK} REQUIRED)
            endif()
            
            message(STATUS "Found framework ${FRAMEWORK} at ${${FRAMEWORK}_LIBRARY}")
            target_link_libraries(${ARG_TARGET} PRIVATE "-framework ${FRAMEWORK}")
        endforeach()
    else ()
        foreach(FRAMEWORK ${ARG_FRAMEWORKS})
            if(ARG_PATHS)
                find_library(${FRAMEWORK}_LIBRARY 
                    NAMES ${FRAMEWORK}
                    PATHS ${ARG_PATHS}
                    NO_DEFAULT_PATH
                )
                if(NOT ${FRAMEWORK}_LIBRARY)
                    find_library(${FRAMEWORK}_LIBRARY 
                        NAMES ${FRAMEWORK}
                    )
                endif()
            else()
                find_library(${FRAMEWORK}_LIBRARY ${FRAMEWORK})
            endif()
            
            if (${FRAMEWORK}_LIBRARY)
                message(STATUS "Found framework ${FRAMEWORK} at ${${FRAMEWORK}_LIBRARY}")
                target_link_libraries(${ARG_TARGET} PRIVATE "-framework ${FRAMEWORK}")
            else ()
                message(STATUS "Framework ${FRAMEWORK} not found, skipping")
            endif()
        endforeach()
    endif()
endfunction()

function(pag_add_library)
    cmake_parse_arguments(ARG "" "TARGET;REQUIRED" "LIBRARIES;PATHS" ${ARGN})

    if (${ARG_REQUIRED})
        foreach(LIBRARY ${ARG_LIBRARIES})
            if(ARG_PATHS)
                find_library(${LIBRARY}_LIBRARY 
                    NAMES ${LIBRARY}
                    PATHS ${ARG_PATHS}
                    REQUIRED
                    NO_DEFAULT_PATH
                )
                if(NOT ${LIBRARY}_LIBRARY)
                    find_library(${LIBRARY}_LIBRARY 
                        NAMES ${LIBRARY}
                        REQUIRED
                    )
                endif()
            else()
                find_library(${LIBRARY}_LIBRARY ${LIBRARY} REQUIRED)
            endif()
            
            message(STATUS "Found library ${LIBRARY} at ${${LIBRARY}_LIBRARY}")
            target_link_libraries(${ARG_TARGET} PRIVATE ${${LIBRARY}_LIBRARY})
        endforeach()
    else ()
        foreach(LIBRARY ${ARG_LIBRARIES})
            if(ARG_PATHS)
                find_library(${LIBRARY}_LIBRARY 
                    NAMES ${LIBRARY}
                    PATHS ${ARG_PATHS}
                    NO_DEFAULT_PATH
                )
                if(NOT ${LIBRARY}_LIBRARY)
                    find_library(${LIBRARY}_LIBRARY 
                        NAMES ${LIBRARY}
                    )
                endif()
            else()
                find_library(${LIBRARY}_LIBRARY ${LIBRARY})
            endif()
            
            if (${LIBRARY}_LIBRARY)
                message(STATUS "Found library ${LIBRARY} at ${${LIBRARY}_LIBRARY}")
                target_link_libraries(${ARG_TARGET} PRIVATE ${${LIBRARY}_LIBRARY})
            else ()
                message(STATUS "Library ${LIBRARY} not found, skipping")
            endif()
        endforeach()
    endif()
endfunction()

function(get_deployqt QT6_PATH deployqt)
    if (PAG_WINDOWS)
        SET(${deployqt} ${QT6_PATH}/../../bin/windeployqt.exe PARENT_SCOPE)
    ELSEIF (PAG_MACOS)
        SET(${deployqt} ${QT6_PATH}/../../bin/macdeployqt PARENT_SCOPE)
    ELSE()
        MESSAGE(FATAL_ERROR "Unsupported Platform: ${CMAKE_SYSTEM_NAME}")
    ENDIF()
endfunction()