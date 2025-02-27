function(pag_common_setting TARGET)
    if (PAG_WINDOWS)
        target_compile_definitions(${TARGET} PRIVATE -DPAG_WINDOWS)
        if (MSVC)
            set_target_properties(${TARGET} PROPERTIES LINK_FLAGS "/OPT:REF /OPT:ICF")
            target_compile_definitions(${TARGET} PRIVATE -DPAG_MSVC)
            target_compile_options(${TARGET} PRIVATE /bigobj)
            target_compile_options(${TARGET} PRIVATE
                "$<$<C_COMPILER_ID:MSVC>:/utf-8>" # 启用 utf-8
                "$<$<CXX_COMPILER_ID:MSVC>:/utf-8>" # 启用 utf-8
                "$<$<CXX_COMPILER_ID:MSVC>:/Zi>" # 符号信息保存到pdb
                "$<$<CXX_COMPILER_ID:MSVC>:/MP>" # 启用多进程编译
                "$<$<CXX_COMPILER_ID:MSVC>:/Zc:__cplusplus>" # 启用C++版本宏
                "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/MDd>"
                "$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/MD>"
            )
        endif()
    elseif (PAG_MACOS)
        target_link_options(${TARGET} PRIVATE -Wl,-dead_strip)
        target_compile_definitions(${TARGET} PRIVATE -DPAG_MACOS)
    else()
        message(FATAL_ERROR "Unsupported Platform: ${CMAKE_SYSTEM_NAME}")
    endif()

    set_target_properties(${TARGET} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
endfunction()

macro(set_default_qt_dir)
    if (CMAKE_PREFIX_PATH MATCHES "")
        if (EXISTS ${CMAKE_SOURCE_DIR}/QTCMAKE.cfg)
            include(${CMAKE_SOURCE_DIR}/QTCMAKE.cfg)
        else ()
            if(PAG_WINDOWS)
                set(QT_DIR "D:/application/qt6/6.8.1/msvc2022_64/lib/cmake" ${QT_DIR})
            elseif(PAG_MACOS)
                set(QT_DIR "/usr/local/opt/qt@6/lib/cmake" ${QT_DIR})
            else()
                message(FATAL_ERROR "Unsupported Platform: ${CMAKE_SYSTEM_NAME}")
            endif()

            set(CMAKE_PREFIX_PATH ${QT_DIR} ${CMAKE_PREFIX_PATH})
        endif()
    endif ()

    message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")

    if (NOT EXISTS ${CMAKE_SOURCE_DIR}/QTCMAKE.cfg)
        file(WRITE ${CMAKE_SOURCE_DIR}/QTCMAKE.cfg "set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})")
    endif()
endmacro()

function(init_tools)
    message(STATUS "┌─ Init Tools Path")
    find_program(pag_python NAMES python3 python REQUIRED)
    find_program(pag_clangformt NAMES clang-format)
    message(STATUS "├─ Python Path: " ${pag_python})
    message(STATUS "├─ ClangFormat Path: " ${pag_clangformt})
    message(STATUS "└─ Init Tools Path Done")

    set(python ${pag_python} CACHE INTERNAL "Python program path")
endfunction(init_tools)