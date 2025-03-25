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
#include <QEvent>
#include <QObject>
#include "rendering/PAGWindow.h"

PAGViewer::PAGViewer(int& argc, char** argv) : QApplication(argc, argv) {
}

auto PAGViewer::event(QEvent* event) -> bool {
  if (event->type() == QEvent::FileOpen) {
    auto openEvent = static_cast<QFileOpenEvent*>(event);
    auto path = openEvent->file();
    openFile(path);
  }
  return QApplication::event(event);
}

auto PAGViewer::openFile(QString path) -> void {
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
    window->open();
    PAGWindow::AllWindows.append(window);
    QObject::connect(window, &PAGWindow::destroyWindow, this, &PAGViewer::onWindowDestroyed,
                     Qt::UniqueConnection);
  }

  window->openFile(path);
}

auto PAGViewer::onWindowDestroyed(PAGWindow* window) -> void {
  PAGWindow::AllWindows.removeOne(window);
  window->deleteLater();
}
