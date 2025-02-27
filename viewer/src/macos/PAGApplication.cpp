#include "PAGApplication.h"
#include <QList>
#include <QEvent>
#include "license/LicenseDialog.h"
#include "rendering/PAGWindow.h"

PAGApplication::PAGApplication(int& argc, char** argv) : QApplication(argc, argv) {
  setApplicationVersion(PAGViewerVersion == "~PAGViewerVersion~"
    ? "1.0.0 (stable)"
    : PAGViewerVersion + " (" + UpdateChannel + ")");
}

auto PAGApplication::event(QEvent* event) -> bool {
  if (event->type() == QEvent::FileOpen) {
    auto* openEvent = dynamic_cast<QFileOpenEvent*>(event);
    auto path = openEvent->file();
    if (!LicenseDialog::isUserAgreeWithLicense()) {
      qDebug() << "User do not agree with the license, just store the file path: " << path;
      waitToOpenFile = path;
      return true;
    }
    openFile(path);
  }
  return QApplication::event(event);
}

auto PAGApplication::openFile(QString path) -> void {
  if (path.isEmpty()) {
    path = waitToOpenFile;
  }
  waitToOpenFile.clear();
  qDebug() << "Open file" << path;

  QList<PAGWindow*> unused_windows;
  PAGWindow* window = nullptr;

  if (PAGWindow::AllWindows.count() > 0 && path.isEmpty()) {
    return;
  }

  for (int i = 0; i < PAGWindow::AllWindows.count(); i++) {
    auto iter = PAGWindow::AllWindows[i];
    QString filePath = iter->getFilePath();
    if (filePath.isEmpty()) {
      unused_windows.append(iter);
      continue;
    }
    if (filePath == path) {
      window = iter;
    }
  }

  if (window == nullptr) {
    for (int i = 0; i < unused_windows.count(); i++) {
      auto iter = unused_windows[i];
      if (i == 0) {
        window = iter;
      } else {
        iter->close();
      }
    }

    if (window == nullptr) {
      window = new PAGWindow();
    }

    window->Open();
    PAGWindow::AllWindows.append(window);
    QObject::connect(window, &PAGWindow::destroyWindow,
                     this, &PAGApplication::onWindowDestroyed, Qt::UniqueConnection);
    QObject::connect(window, &PAGWindow::openPAGFile,
                     this, &PAGApplication::openFile);
  }

  window->openFile(path);
}

auto PAGApplication::onWindowDestroyed(PAGWindow* window) -> void {
  PAGWindow::AllWindows.removeOne(window);
  window->deleteLater();
}

auto PAGApplication::applicationMessage(int instanceId, QByteArray message) -> void {
  QString cmd = QString::fromUtf8(message);
  if (!LicenseDialog::isUserAgreeWithLicense()) {
    qDebug() << "User do not agree with the license, just store the file path: " << cmd;
    waitToOpenFile = cmd;
    return;
  }
  openFile(cmd);
}

