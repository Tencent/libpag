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

#include "PAGViewer.h"
#include <QEvent>
#include <QObject>
#include "rendering/PAGWindow.h"
#include "version.h"

namespace pag {

PAGViewer::PAGViewer(int& argc, char** argv) : QApplication(argc, argv) {
  setApplicationVersion((AppVersion + (UpdateChannel == "beta" ? "-beta" : "")).data());
  checkUpdateModel = std::make_unique<PAGCheckUpdateModel>();
}

bool PAGViewer::event(QEvent* event) {
  if (event->type() == QEvent::FileOpen) {
    auto openEvent = static_cast<QFileOpenEvent*>(event);
    auto path = openEvent->file();
    openFile(path);
  }
  return QApplication::event(event);
}

void PAGViewer::openFile(QString path) {
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

PAGCheckUpdateModel* PAGViewer::getCheckUpdateModel() {
  return checkUpdateModel.get();
}

void PAGViewer::onWindowDestroyed(PAGWindow* window) {
  PAGWindow::AllWindows.removeOne(window);
  window->deleteLater();
}

}  // namespace pag
