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

#include "PAGViewer.h"
#include <QObject>

PAGViewer::PAGViewer(int& argc, char** argv) : QApplication(argc, argv) {
}

bool PAGViewer::event(QEvent* event) {
  if (event->type() == QEvent::FileOpen) {
    auto openEvent = static_cast<QFileOpenEvent*>(event);
    auto path = openEvent->file();
    OpenFile(path);
  }
  return QApplication::event(event);
}

void PAGViewer::OpenFile(QString path) {
  PAGWindow* window = nullptr;
  for (int i = 0; i < PAGWindow::AllWindows.count(); i++) {
    auto win = PAGWindow::AllWindows[i];
    auto fileInWindow = win->getFilePath();
    if (fileInWindow.isEmpty() || fileInWindow == path) {
      window = win;
      break;
    }
  }

  if (!window) {
    window = new PAGWindow();
    window->Open();
    PAGWindow::AllWindows.append(window);
    QObject::connect(window, &PAGWindow::DestroyWindow, this, &PAGViewer::onWindowDestroyed,
                     Qt::UniqueConnection);
  }

  window->OpenFile(path);
}

void PAGViewer::onWindowDestroyed(PAGWindow* window) {
  PAGWindow::AllWindows.removeOne(window);
  window->deleteLater();
}
