cmake_minimum_required(VERSION 3.13)
project(PAGViewer)

execute_process(COMMAND sh ${CMAKE_SOURCE_DIR}/sync_deps.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
execute_process(COMMAND sh ${CMAKE_SOURCE_DIR}/../sync_deps.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/../)

include(cmake/global_setting.cmake)
include(cmake/set_info.cmake)
include(cmake/utils.cmake)
include(tools/vendor_tools/vendor.cmake)

init_tools()
set_default_qt_dir()
update_version()

add_subdirectory(src)