cmake_minimum_required(VERSION 3.13)
project(PAGViewer)

include(cmake/global_setting.cmake)
include(cmake/set_info.cmake)
include(tools/vendor_tools/vendor.cmake)

init_tools()
set_default_qt_dir()

add_subdirectory(src)