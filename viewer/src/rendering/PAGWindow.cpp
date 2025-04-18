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

#include "PAGWindow.h"
#include <QQmlContext>
#include <QThread>
#include "PAGWindowHelper.h"
#include "task/PAGTaskFactory.h"

namespace pag {

QList<PAGWindow*> PAGWindow::AllWindows;

PAGWindow::PAGWindow(QObject* parent) : QObject(parent) {
}

PAGWindow::~PAGWindow() = default;

auto PAGWindow::openFile(QString path) -> void {
  bool result = pagView->setFile(path);
  if (!result) {
    return;
  }
  filePath = path;
  window->raise();
  window->requestActivate();
}

auto PAGWindow::onPAGViewerDestroyed() -> void {
  Q_EMIT destroyWindow(this);
}

auto PAGWindow::open() -> void {
  engine = std::make_unique<QQmlApplicationEngine>();
  windowHelper = std::make_unique<PAGWindowHelper>();

  auto context = engine->rootContext();
  context->setContextProperty("windowHelper", windowHelper.get());

  engine->load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));

  window = static_cast<QQuickWindow*>(engine->rootObjects().at(0));
  window->setPersistentGraphics(true);
  window->setPersistentSceneGraph(true);
  window->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);

  pagView = window->findChild<pag::PAGView*>("pagView");
  auto* taskFactory = window->findChild<PAGTaskFactory*>("taskFactory");

  connect(window, SIGNAL(closing(QQuickCloseEvent*)), this, SLOT(onPAGViewerDestroyed()),
          Qt::QueuedConnection);
  connect(pagView, &PAGView::fileChanged, taskFactory, &PAGTaskFactory::resetFile);
}

auto PAGWindow::getFilePath() -> QString {
  return filePath;
}

}  // namespace pag
