file(GLOB_RECURSE CLI_FILES src/*.cpp src/*.h)

add_executable(pagx-cli ${CLI_FILES})
set_target_properties(pagx-cli PROPERTIES OUTPUT_NAME pagx)

target_include_directories(pagx-cli PRIVATE
        src
        ${PAG_INCLUDES}
)

target_compile_options(pagx-cli PRIVATE ${PAG_COMPILE_OPTIONS})
target_link_libraries(pagx-cli pagx pag ${PAG_SHARED_LIBS})
