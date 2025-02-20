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


#include <QtWidgets/QApplication>
#include <QtGui/QFont>
#include <QtQuick/QQuickWindow>
#include <QtGui/QSurfaceFormat>
#include "EnvConfig.h"

static int gAppIndex = 0;
static QApplication* gCurrentQtApp = nullptr;

QApplication* SetupQT() {
  QApplication::setAttribute(Qt::AA_PluginApplication, true);
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  QSurfaceFormat defaultFormat = QSurfaceFormat();
  defaultFormat.setRenderableType(QSurfaceFormat::RenderableType::OpenGL);
  defaultFormat.setVersion(3, 2);
  defaultFormat.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(defaultFormat);

  std::vector<std::string> fallbackList;
#ifdef WIN32
  QFont defaultFonts("Microsoft Yahei");
  defaultFonts.setStyleHint(QFont::SansSerif);
  QApplication::setFont(defaultFonts);
  fallbackList = {"Microsoft YaHei"};
#else
  QFont defaultFonts("Helvetica Neue,PingFang SC");
  QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
  defaultFonts.setStyleHint(QFont::SansSerif);
  QApplication::setFont(defaultFonts);
  fallbackList = {"PingFang SC", "Apple Color Emoji"};
#endif

  int argc = 0;
  QApplication* app = new QApplication(argc, nullptr);
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
  QString appName("PAG-Exporter-");
  appName.append(QString::number(gAppIndex));
  app->setObjectName(appName);
  gCurrentQtApp = app;
  printf("SetupQT objectName:%s\n", appName.toStdString().c_str());
  gAppIndex++;
  return app;
}

QApplication* CurrentUseQtApp() {
  return gCurrentQtApp;
}