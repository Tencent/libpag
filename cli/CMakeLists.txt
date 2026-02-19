# libxml2 is provided by macOS SDK. Provide hints when find_package cannot locate it.
if (APPLE)
    execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE _MACOS_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(LIBXML2_INCLUDE_DIR "${_MACOS_SDK_PATH}/usr/include/libxml2" CACHE PATH "" FORCE)
    set(LIBXML2_LIBRARY "${_MACOS_SDK_PATH}/usr/lib/libxml2.tbd" CACHE FILEPATH "" FORCE)
endif ()
find_package(LibXml2 REQUIRED)

# Generate pagx_xsd.h from spec/pagx.xsd
set(PAGX_XSD_INPUT "${CMAKE_CURRENT_SOURCE_DIR}/../spec/pagx.xsd")
set(PAGX_XSD_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/pagx_xsd.h")
add_custom_command(
        OUTPUT "${PAGX_XSD_HEADER}"
        COMMAND ${CMAKE_COMMAND}
        -DXSD_INPUT=${PAGX_XSD_INPUT}
        -DHEADER_OUTPUT=${PAGX_XSD_HEADER}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/generate_xsd_header.cmake
        DEPENDS "${PAGX_XSD_INPUT}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_xsd_header.cmake"
        COMMENT "Generating pagx_xsd.h from spec/pagx.xsd"
)
add_custom_target(pagx_xsd_header DEPENDS "${PAGX_XSD_HEADER}")

# PAG_INCLUDES contains paths relative to the project root. Convert them to absolute paths
# so they resolve correctly from this subdirectory.
set(CLI_PAG_INCLUDES "")
foreach (_inc ${PAG_INCLUDES})
    if (IS_ABSOLUTE "${_inc}")
        list(APPEND CLI_PAG_INCLUDES "${_inc}")
    else ()
        list(APPEND CLI_PAG_INCLUDES "${CMAKE_SOURCE_DIR}/${_inc}")
    endif ()
endforeach ()

set(CLI_INCLUDE_DIRS
        src
        ${CLI_PAG_INCLUDES}
        ${CMAKE_CURRENT_BINARY_DIR}/generated
        ${LIBXML2_INCLUDE_DIR}
)

set(CLI_LINK_LIBS pagx pag ${PAG_SHARED_LIBS} LibXml2::LibXml2)

# CLI library (all command implementations, excluding main.cpp)
file(GLOB_RECURSE CLI_LIB_FILES src/*.cpp src/*.h)
list(FILTER CLI_LIB_FILES EXCLUDE REGEX "main\\.cpp$")

add_library(pagx-cli-lib STATIC ${CLI_LIB_FILES})
add_dependencies(pagx-cli-lib pagx_xsd_header)
target_include_directories(pagx-cli-lib PUBLIC ${CLI_INCLUDE_DIRS})
target_compile_options(pagx-cli-lib PRIVATE ${PAG_COMPILE_OPTIONS})
target_link_libraries(pagx-cli-lib ${CLI_LINK_LIBS})

# CLI executable
add_executable(pagx-cli src/main.cpp)
set_target_properties(pagx-cli PROPERTIES OUTPUT_NAME pagx)
target_compile_options(pagx-cli PRIVATE ${PAG_COMPILE_OPTIONS})
target_link_libraries(pagx-cli pagx-cli-lib)

# CLI tests
set(GOOGLE_TEST_DIR ${TGFX_DIR}/third_party/googletest/googletest)
file(GLOB_RECURSE CLI_TEST_FILES test/*.cpp)
if (CLI_TEST_FILES)
    add_executable(pagx-cli-test ${CLI_TEST_FILES})
    add_dependencies(pagx-cli-test test-vendor)
    target_include_directories(pagx-cli-test PRIVATE
            ${GOOGLE_TEST_DIR} ${GOOGLE_TEST_DIR}/include
            ${CMAKE_SOURCE_DIR}/test/src
    )
    target_compile_options(pagx-cli-test PRIVATE ${PAG_COMPILE_OPTIONS})
    find_vendor_libraries(test-vendor STATIC CLI_TEST_VENDOR_LIBS)
    target_link_libraries(pagx-cli-test pagx-cli-lib ${CLI_TEST_VENDOR_LIBS})
endif ()
