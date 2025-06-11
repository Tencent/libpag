/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "WindowManager.h"
#include <QDesktopServices>
#include <QFile>
#include <QUrl>
#include <QtGui/QFont>
#include <QtGui/QSurfaceFormat>
#include <QtQuick/QQuickWindow>
#include <QtWidgets/QApplication>
#include "utils/AEHelper.h"

namespace exporter {

WindowManager& WindowManager::getInstance() {
  static WindowManager instance;
  return instance;
}

WindowManager::WindowManager() {
  AEHelper::RunScriptPreWarm();
  initializeQtEnvironment();
}

void WindowManager::showPanelExporterWindow() {
}

void WindowManager::showPAGConfigWindow() {
}

void WindowManager::showExportPreviewWindow() {
}

void WindowManager::initializeQtEnvironment() {
  QApplication::setAttribute(Qt::AA_PluginApplication, true);
  QSurfaceFormat defaultFormat = QSurfaceFormat();
  defaultFormat.setRenderableType(QSurfaceFormat::RenderableType::OpenGL);
  defaultFormat.setVersion(3, 2);
  defaultFormat.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(defaultFormat);

#ifdef WIN32
  QFont defaultFonts("Microsoft Yahei");
  defaultFonts.setStyleHint(QFont::SansSerif);
  QApplication::setFont(defaultFonts);
#else
  QFont defaultFonts("Helvetica Neue,PingFang SC");
  QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
  defaultFonts.setStyleHint(QFont::SansSerif);
  QApplication::setFont(defaultFonts);
#endif
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
}

}  // namespace exporter
