execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/../ ${CMAKE_SOURCE_DIR}/third_party/libpag)
execute_process(COMMAND sh ${CMAKE_SOURCE_DIR}/sync_deps.sh WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})

file(GLOB_RECURSE PAG_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.mm
)

file(GLOB_RECURSE PAG_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.hpp
)

if (PAG_WINDOWS)
  MESSAGE("This is PAG_WINDOWS")
  file(GLOB_RECURSE PAG_PLATFORM_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/macos/*.cpp
      ${CMAKE_CURRENT_SOURCE_DIR}/macos/*.cpp
  )
  list(REMOVE_ITEM PAG_SRCS ${PAG_PLATFORM_SRCS})

  file(GLOB_RECURSE PAG_PLATFORM_INCLUDES
      ${CMAKE_CURRENT_SOURCE_DIR}/mcaos/*.h
  )
  list(REMOVE_ITEM PAG_INCLUDES ${PAG_PLATFORM_INCLUDES})
elseif (PAG_MACOS)
  file(GLOB_RECURSE PAG_PLATFORM_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.mm
      ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.cpp
  )
  list(REMOVE_ITEM PAG_SRCS ${PAG_PLATFORM_SRCS})

  file(GLOB_RECURSE PAG_PLATFORM_INCLUDES
      ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.h
  )
  list(REMOVE_ITEM PAG_INCLUDES ${PAG_PLATFORM_INCLUDES})
endif()

add_executable(PAGViewer ${PAG_SRCS})
pag_common_setting(PAGViewer)
target_include_directories(PAGViewer PRIVATE ${PAG_INCLUDES})

set(QT_MODULES
    Qml
    Gui
    Core
    Quick
    OpenGL
    Widgets
    Network
    Multimedia
    Core5Compat
    QuickControls2)
pag_add_qt6_library(MODULES ${QT_MODULES}
                    TARGET PAGViewer
                    REQUIRED True)
find_package(Qt6)
qt6_add_resources(QT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources/res.qrc)
target_sources(PAGViewer PRIVATE ${QT_RESOURCES})

add_vendor_target(nlohman-json STATIC_VENDORS nlohman-json CONFIG_DIR ${CMAKE_SOURCE_DIR})
add_vendor_target(sodium STATIC_VENDORS sodium CONFIG_DIR ${CMAKE_SOURCE_DIR})
add_vendor_target(pag STATIC_VENDORS libpag CONFIG_DIR ${CMAKE_SOURCE_DIR})
if (PAG_WINDOWS)
  add_vendor_target(winsparkle STATIC_VENDORS winsparkle CONFIG_DIR ${CMAKE_SOURCE_DIR})
  add_dependencies(PAGViewer nlohman-json sodium winsparkle libpag)

  target_include_directories(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/nlohman-json/include)
  target_include_directories(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/include)
  # TODO

  set_target_properties(PAGViewer PROPERTIES WIN32_EXECUTABLE TRUE)
elseif (PAG_MACOS)
  set(SYSTEM_FRAMEWORKS
      AGL
      Cocoa
      IOKit
      OpenGL
      CoreMedia
      CoreVideo
      QuartzCore
      Foundation
      VideoToolbox
      ApplicationServices
  )
  pag_add_framework(FRAMEWORKS ${SYSTEM_FRAMEWORKS}
                    TARGET PAGViewer
                    REQUIRED True)

  set(SYSTEM_LIBRARIES
      xml2
      compression
  )
  pag_add_library(LIBRARIES ${SYSTEM_LIBRARIES}
                  TARGET PAGViewer
                  REQUIRED True)

  set(SYSTEM_LIBRARIES_OPTIONAL
      c
      iconv
      iconv-2
  )
  pag_add_library(LIBRARIES ${SYSTEM_LIBRARIES_OPTIONAL}
                  TARGET PAGViewer
                  REQUIRED False)

  add_vendor_target(log4qt STATIC_VENDORS log4qt CONFIG_DIR ${CMAKE_SOURCE_DIR})
  add_vendor_target(sparkle STATIC_VENDORS sparkle CONFIG_DIR ${CMAKE_SOURCE_DIR})
  add_dependencies(PAGViewer log4qt nlohman-json sodium sparkle pag)

  target_include_directories(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/nlohman-json/include)
  target_include_directories(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/include)
  target_include_directories(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/include)
  target_include_directories(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/../src)
  target_include_directories(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/out/rttr/mac/include)
  target_include_directories(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/tgfx/include)

  target_link_libraries(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/lib/${CMAKE_BUILD_TYPE}/libsodium.a)
  target_link_libraries(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/log4qt/lib/${CMAKE_BUILD_TYPE}/liblog4qt.a)
  target_link_libraries(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libpag.a)
  target_link_libraries(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libpag-vendor.a)
  target_link_libraries(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libtgfx-vendor.a)
  target_link_libraries(PAGViewer PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sparkle/lib/Release/Sparkle.framework)

  set_target_properties(PAGViewer PROPERTIES MACOSX_BUNDLE TRUE)  
endif()