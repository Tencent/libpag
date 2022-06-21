/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <QApplication>
#include <QFileInfo>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QThread>
#include "PAGView.h"
#include "PAGViewer.h"
#include "qobject.h"

int main(int argc, char* argv[]) {
  QApplication::setApplicationName("PAGViewer");
  QApplication::setOrganizationName("CCC");
  QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

  QSurfaceFormat defaultFormat = QSurfaceFormat();
  defaultFormat.setRenderableType(QSurfaceFormat::RenderableType::OpenGL);
  defaultFormat.setVersion(3, 2);
  defaultFormat.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(defaultFormat);

  PAGViewer app(argc, argv);
  app.setWindowIcon(QIcon(":/images/window-icon.png"));
  qmlRegisterType<pag::PAGView>("PAG", 1, 0, "PAGView");
  auto rootPath = QApplication::applicationDirPath();
#ifdef WIN32
  rootPath = QFileInfo(rootPath + "/../../").absolutePath();
#else
  rootPath = QFileInfo(rootPath + "/../../../../../").absolutePath();
#endif
  app.OpenFile(rootPath + "/assets/test2.pag");
  return app.exec();
}
