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
  bool isPagx = path.toLower().endsWith(".pagx");
  PAGWindow* window = nullptr;
  for (int i = 0; i < PAGWindow::AllWindows.count(); i++) {
    auto win = PAGWindow::AllWindows[i];
    auto fileInWindow = win->getFilePath();
    if (!path.isEmpty() && fileInWindow == path) {
      window = win;
      break;
    }
    // Reuse an empty window for any file type. The QML layer switches the view component
    // (PAGView vs PAGXView) automatically in MainForm.loadFile(), so a PAGX file can reuse
    // a fresh PAG window instead of forcing a new one. A window reports hasContent() == false
    // when it never loaded a file or its last load failed, so both cases are reusable.
    // isReady() ensures the QML window exists; otherwise PAGWindow::openFile() would silently
    // drop the request.
    if (!win->hasContent() && win->isReady()) {
      window = win;
      break;
    }
  }

  if (!window) {
    window = new PAGWindow();
    PAGWindow::AllWindows.append(window);
    QObject::connect(window, &PAGWindow::destroyWindow, this, &PAGViewer::onWindowDestroyed,
                     Qt::UniqueConnection);
    QString viewType = isPagx ? "pagx" : "pag";
    window->open(viewType);
  }

  if (!path.isEmpty()) {
    window->openFile(path);
  }
}

PAGCheckUpdateModel* PAGViewer::getCheckUpdateModel() {
  return checkUpdateModel.get();
}

void PAGViewer::onWindowDestroyed(PAGWindow* window) {
  PAGWindow::AllWindows.removeOne(window);
  window->deleteLater();
}

}  // namespace pag
