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

QList<PAGWindow*> PAGWindow::AllWindows;

PAGWindow::PAGWindow(QObject* parent) : QObject(parent) {
}

PAGWindow::~PAGWindow() {
  delete pagView;
  delete engine;
}

void PAGWindow::onPAGViewerDestroyed() {
  Q_EMIT DestroyWindow(this);
}

void PAGWindow::Open() {
  engine = new QQmlApplicationEngine;
  engine->load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
  window = static_cast<QQuickWindow*>(engine->rootObjects().at(0));
  pagView = window->findChild<pag::PAGView*>("pagView");
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
  window->setPersistentGraphics(true);
#else
  window->setPersistentOpenGLContext(true);
#endif
  window->setPersistentSceneGraph(true);
  connect(window, SIGNAL(closing(QQuickCloseEvent*)), this, SLOT(onPAGViewerDestroyed()),
          Qt::QueuedConnection);
}

void PAGWindow::OpenFile(QString path) {
  auto strPath = std::string(path.toLocal8Bit());
  if (path.startsWith("file://")) {
    strPath = std::string(QUrl(path).toLocalFile().toLocal8Bit());
  }
  auto byteData = pag::ByteData::FromPath(strPath);
  if (byteData == nullptr) {
    return;
  }
  auto pagFile = pag::PAGFile::Load(byteData->data(), byteData->length());
  if (pagFile == nullptr) {
    return;
  }
  filePath = path;
  pagView->setFile(pagFile);
  auto width = pagFile->width();
  auto height = pagFile->height();
  if (height > 800) {
    float scale = static_cast<float>(800) / static_cast<float>(height);
    height = 800;
    width = static_cast<int>(static_cast<float>(width) * scale);
  }
  window->resize(width, height);
  window->raise();
  window->requestActivate();
}
