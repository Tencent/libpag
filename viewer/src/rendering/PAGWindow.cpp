#include "PAGWindow.h"
#include <QQmlContext>
#include <QCoreApplication>
#include "license/LicenseDialog.h"

PAGWindow::PAGWindow(QObject* parent) : QObject(parent) {

}

PAGWindow::~PAGWindow() {
  qDebug() << "Destory PAGWindow";
}
auto PAGWindow::Open() -> void {
  qmlEngine = new QQmlApplicationEngine();
  windowHelper = new WindowHelper();
  // TODO

  auto context = qmlEngine->rootContext();
  context->setContextProperty("windowHelper", windowHelper);
  context->setContextProperty("licenseUrl", LicenseDialog::licenseUrl);
  context->setContextProperty("privacyUrl", LicenseDialog::privacyUrl);

#if defined(QT_DEBUG)
  QString qmlPath = QCoreApplication::applicationDirPath() + "../../../../../../../src/resources/qml/MainWindow.qml";
  qmlEngine->load(QUrl(qmlPath));
#else
  engine->load(QUrl(QStringLiteral("qrc:/qml/MainWindow.qml"));
#endif
  quickWindow = static_cast<QQuickWindow* >(qmlEngine->rootObjects().at(0));
  quickWindow->setPersistentGraphics(true);
  quickWindow->setPersistentSceneGraph(true);
  quickWindow->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);
  // TODO
}

auto PAGWindow::getFilePath() -> QString {
  // TODO
  return {""};
  // return pagViewer->getFilePath();
}

auto PAGWindow::getEngine() -> QQmlApplicationEngine* {
  return qmlEngine;
}

auto PAGWindow::close() -> void {
  if (!PAGWindow::AllWindows.contains(this)) {
    return;
  }
  
  if (!windowHelper) {
    return;
  }
  quickWindow->close();
  this->deleteLater();
  PAGWindow::AllWindows.removeOne(this);
}

auto PAGWindow::openFile(const QString& path) -> void {
  if (!path.isEmpty()) {
    // TODO
    // pagViewer->setFile(path);
  }
  quickWindow->raise();
  quickWindow->requestActivate();
}

auto PAGWindow::onPAGViewerDestroyed() -> void {
  Q_EMIT destroyWindow(this);
}

auto PAGWindow::onShowVideoFramesChanged(bool show) -> void {
  for (int i = 0; i < PAGWindow::AllWindows.count(); i++) {
    auto iter = PAGWindow::AllWindows[i];
    // TODO
    // iter->pagViewer->setShowVideoFrames(show);
  }
}

auto PAGWindow::openProject(const QString& path) -> void {
  Q_EMIT openPAGFile(QString(""));
}

QList<PAGWindow* > PAGWindow::AllWindows;