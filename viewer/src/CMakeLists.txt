execute_process(COMMAND sh ${CMAKE_SOURCE_DIR}/sync_deps.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

file(GLOB_RECURSE PAG_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/license/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering/*.cpp
)

file(GLOB_RECURSE PAG_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/license/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rendering/*.h
)

if (PAG_WINDOWS)
  file(GLOB_RECURSE PAG_PLATFORM_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.cpp
  )
  list(APPEND PAG_SRCS ${PAG_PLATFORM_SRCS})

  file(GLOB_RECURSE PAG_PLATFORM_INCLUDES
      ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.h
  )
  list(APPEND PAG_INCLUDES ${PAG_PLATFORM_INCLUDES})
elseif (PAG_MACOS)
  file(GLOB_RECURSE PAG_PLATFORM_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/macos/*.cpp
  )
  list(APPEND PAG_SRCS ${PAG_PLATFORM_SRCS})

  file(GLOB_RECURSE PAG_PLATFORM_INCLUDES
      ${CMAKE_CURRENT_SOURCE_DIR}/macos/*.h
  )
  list(APPEND PAG_INCLUDES ${PAG_PLATFORM_INCLUDES})
endif()

add_executable(PAGViewer ${PAG_SRCS})
pag_common_setting(PAGViewer)
target_include_directories(PAGViewer PRIVATE ${PAG_INCLUDES})

set(QT_MODULES Qml Gui Core Quick OpenGL Widgets Network Multimedia Core5Compat QuickControls2)
foreach(MODULE ${QT_MODULES})
    find_package(Qt6${MODULE} REQUIRED)
    target_link_libraries(PAGViewer PRIVATE Qt6::${MODULE})
    target_include_directories(PAGViewer PRIVATE ${Qt6${MODULE}_INCLUDE_DIRS})
endforeach()
qt6_add_resources(QT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources/res.qrc)
target_sources(PAGViewer PRIVATE ${QT_RESOURCES})

add_vendor_target(nlohman-json
  STATIC_VENDORS nlohman-json
  CONFIG_DIR ${CMAKE_SOURCE_DIR})
add_vendor_target(sodium
  STATIC_VENDORS sodium
  CONFIG_DIR ${CMAKE_SOURCE_DIR})
if (PAG_WINDOWS)
  add_vendor_target(winsparkle
    STATIC_VENDORS winsparkle
    CONFIG_DIR ${CMAKE_SOURCE_DIR})
  add_dependencies(PAGViewer nlohman-json sodium winsparkle)

  set_target_properties(PAGViewer PROPERTIES WIN32_EXECUTABLE TRUE)
elseif (PAG_MACOS)
  add_vendor_target(log4qt
    STATIC_VENDORS log4qt
    CONFIG_DIR ${CMAKE_SOURCE_DIR})
  add_vendor_target(sparkle
    STATIC_VENDORS sparkle
    CONFIG_DIR ${CMAKE_SOURCE_DIR})
  add_dependencies(PAGViewer log4qt nlohman-json sodium sparkle)

  set_target_properties(PAGViewer PROPERTIES MACOSX_BUNDLE TRUE)
endif()