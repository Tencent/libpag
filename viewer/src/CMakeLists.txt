set(APP_NAME PAGViewer)

file(GLOB_RECURSE PAG_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.mm
)

file(GLOB_RECURSE PAG_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*/*.hpp
)

if (PAG_WINDOWS)
  execute_process(COMMAND cmd /c "${CMAKE_SOURCE_DIR}/tools/build_tools/libpag/create_symbol_link.bat" "${CMAKE_SOURCE_DIR}/third_party/libpag" "${CMAKE_SOURCE_DIR}/../")
  file(GLOB_RECURSE PAG_PLATFORM_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/macos/*.h
      ${CMAKE_CURRENT_SOURCE_DIR}/macos/*.mm
      ${CMAKE_CURRENT_SOURCE_DIR}/macos/*.cpp
  )
  list(REMOVE_ITEM PAG_SRCS ${PAG_PLATFORM_SRCS})

  file(GLOB_RECURSE PAG_PLATFORM_INCLUDES
      ${CMAKE_CURRENT_SOURCE_DIR}/macos
  )
  list(REMOVE_ITEM PAG_INCLUDES ${PAG_PLATFORM_INCLUDES})
elseif (PAG_MACOS)
  execute_process(COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/../ ${CMAKE_SOURCE_DIR}/third_party/libpag)
  file(GLOB_RECURSE PAG_PLATFORM_SRCS
      ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.h
      ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.mm
      ${CMAKE_CURRENT_SOURCE_DIR}/windows/*.cpp
  )
  list(REMOVE_ITEM PAG_SRCS ${PAG_PLATFORM_SRCS})

  file(GLOB_RECURSE PAG_PLATFORM_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/windows
  )
  list(REMOVE_ITEM PAG_INCLUDES ${PAG_PLATFORM_INCLUDES})
endif()

add_executable(${APP_NAME} ${PAG_SRCS})
pag_common_setting(${APP_NAME})
target_include_directories(${APP_NAME} PRIVATE ${PAG_INCLUDES})

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
                    TARGET ${APP_NAME}
                    REQUIRED True)
find_package(Qt6)
qt6_add_resources(QT_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/resources/res.qrc)
target_sources(${APP_NAME} PRIVATE ${QT_RESOURCES})

add_vendor_target(nlohman-json STATIC_VENDORS nlohman-json CONFIG_DIR ${CMAKE_SOURCE_DIR})
add_vendor_target(sodium STATIC_VENDORS sodium CONFIG_DIR ${CMAKE_SOURCE_DIR})
add_vendor_target(pag STATIC_VENDORS libpag CONFIG_DIR ${CMAKE_SOURCE_DIR})

get_deployqt(${CMAKE_PREFIX_PATH} deployqt)

if (PAG_WINDOWS)
  set(SYSTEM_LIBRARIES
      bcrypt
      ws2_32
      opengl32
  )
  # need to set corresponding path
  pag_add_library(LIBRARIES ${SYSTEM_LIBRARIES}
                  PATHS "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.22621.0/um/x64"
                  TARGET ${APP_NAME}
                  REQUIRED True)

  add_vendor_target(winsparkle STATIC_VENDORS winsparkle CONFIG_DIR ${CMAKE_SOURCE_DIR})
  add_dependencies(${APP_NAME} nlohman-json sodium winsparkle pag)

  add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMENT "Copy librarys to output directory"
    COMMAND ${deployqt} --qml --qmldir ${CMAKE_SOURCE_DIR}/src/resources/qml --quick  --quickcontrols2 --no-translations $<TARGET_FILE:${APP_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_SOURCE_DIR}/third_party/out/sodium/lib/${CMAKE_BUILD_TYPE}/libsodium.dll
      $<TARGET_FILE_DIR:${APP_NAME}>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${CMAKE_SOURCE_DIR}/third_party/out/winsparkle/lib/Release/WinSparkle.dll
      $<TARGET_FILE_DIR:${APP_NAME}>
  )

  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../src)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/out/rttr/mac/include)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/tgfx/include)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/include)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/include)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/winsparkle/include)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/nlohman-json/include)

  target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/lib/${CMAKE_BUILD_TYPE}/libsodium.lib)
  target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libpag.lib)
  target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libpag-vendor.lib)
  target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libtgfx-vendor.lib)
  if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/winsparkle/lib/${CMAKE_BUILD_TYPE}/WinSparkle.lib)
  endif ()

  set_target_properties(${APP_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
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
                    TARGET ${APP_NAME}
                    REQUIRED True)

  set(SYSTEM_LIBRARIES
      xml2
      compression
  )
  pag_add_library(LIBRARIES ${SYSTEM_LIBRARIES}
                  TARGET ${APP_NAME}
                  REQUIRED True)

  set(SYSTEM_LIBRARIES_OPTIONAL
      c
      iconv
      iconv-2
  )
  pag_add_library(LIBRARIES ${SYSTEM_LIBRARIES_OPTIONAL}
                  TARGET ${APP_NAME}
                  REQUIRED False)

  add_vendor_target(log4qt STATIC_VENDORS log4qt CONFIG_DIR ${CMAKE_SOURCE_DIR})
  add_vendor_target(sparkle STATIC_VENDORS sparkle CONFIG_DIR ${CMAKE_SOURCE_DIR})
  add_dependencies(${APP_NAME} sodium log4qt nlohman-json sparkle pag)

  add_custom_command(TARGET ${APP_NAME} POST_BUILD
  COMMENT "Copy librarys to output directory"
  COMMAND ${deployqt} $<TARGET_BUNDLE_DIR:${APP_NAME}> -qmldir=${CMAKE_SOURCE_DIR}/src/resources/qml
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_SOURCE_DIR}/third_party/out/sparkle/lib/Release/Sparkle.framework
    $<TARGET_BUNDLE_DIR:${APP_NAME}>/Contents/Frameworks
  )

  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../src)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/out/rttr/mac/include)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/../third_party/tgfx/include)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/include)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/include)
  target_include_directories(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/nlohman-json/include)

  target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sodium/lib/${CMAKE_BUILD_TYPE}/libsodium.a)
  target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/log4qt/lib/${CMAKE_BUILD_TYPE}/liblog4qt.a)
  target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libpag.a)
  target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libpag-vendor.a)
  target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/libpag/lib/${CMAKE_BUILD_TYPE}/libtgfx-vendor.a)
  target_link_libraries(${APP_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/third_party/out/sparkle/lib/Release/Sparkle.framework)

  set_target_properties(${APP_NAME} PROPERTIES MACOSX_BUNDLE TRUE)
endif()