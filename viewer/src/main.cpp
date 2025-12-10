/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <QFileInfo>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include "PAGUpdater.h"
#include "PAGViewer.h"
#include "maintenance/PluginInstallerModel.h"
#include "profiling/PAGRunTimeDataModel.h"
#include "rendering/PAGView.h"
#include "task/PAGTaskFactory.h"

int main(int argc, char* argv[]) {
  bool cpuMode = false;
  std::string filePath;

  for (int i = 0; i < argc; i++) {
    auto arg = std::string(argv[i]);
    auto lowerArg = arg;
    std::transform(lowerArg.begin(), lowerArg.end(), lowerArg.begin(), ::tolower);
    if (lowerArg == "-cpu") {
      cpuMode = true;
    } else if (argc > 1) {
      filePath = arg;
    }
  }

  if (cpuMode) {
    QApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
  }

  QApplication::setApplicationName("PAGViewer");
  QApplication::setOrganizationName("Tencent");
  QSurfaceFormat defaultFormat = QSurfaceFormat();
  defaultFormat.setRenderableType(QSurfaceFormat::RenderableType::OpenGL);
  defaultFormat.setVersion(3, 2);
  defaultFormat.setProfile(QSurfaceFormat::CoreProfile);
  QSurfaceFormat::setDefaultFormat(defaultFormat);
  QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);
  QQuickStyle::setStyle("Universal");
  QFont defaultFonts("Helvetica Neue,PingFang SC");
  defaultFonts.setStyleHint(QFont::SansSerif);
  QApplication::setFont(defaultFonts);
  std::vector<std::string> fallbackList = {"PingFang SC", "Apple Color Emoji", "Microsoft YaHei"};
  pag::PAGFont::SetFallbackFontNames(fallbackList);

  pag::PAGViewer app(argc, argv);
  QApplication::setWindowIcon(QIcon(":/images/window-icon.png"));
  qmlRegisterType<pag::PAGView>("PAG", 1, 0, "PAGView");
  qmlRegisterType<pag::PAGTaskFactory>("PAG", 1, 0, "PAGTaskFactory");
  qmlRegisterType<pag::PluginInstallerModel>("PAG", 1, 0, "PluginInstallerModel");
  app.openFile(QString::fromLocal8Bit(filePath.data()));

  pag::InitUpdater();

  return QApplication::exec();
}
