#include "PAGWindow.h"
#include <QFileInfo>
#include <QQmlContext>
#include <QCoreApplication>
#include "license/LicenseDialog.h"

PAGWindow::PAGWindow(QObject* parent) : QObject(parent) {

}

PAGWindow::~PAGWindow() {
  qDebug() << "Destory PAGWindow";
  delete qmlEngine;
  delete windowHelper;
  delete languageModel;
  delete fileInfoModel;
  delete runTimeModelManager;
}
auto PAGWindow::Open() -> void {
  qmlEngine = new QQmlApplicationEngine();
  windowHelper = new PAGWindowHelper();
  languageModel = new PAGLanguageModel();
  fileInfoModel = new PAGFileInfoModel();
  benchmarkModel = new PAGBenchmarkModel();
  // TODO

  auto context = qmlEngine->rootContext();
  context->setContextProperty("windowManager", this);
  context->setContextProperty("windowHelper", windowHelper);
  context->setContextProperty("languageModel", languageModel);
  context->setContextProperty("fileInfoModel", fileInfoModel);
  context->setContextProperty("benchmarkModel", benchmarkModel);
  context->setContextProperty("licenseUrl", LicenseDialog::licenseUrl);
  context->setContextProperty("privacyUrl", LicenseDialog::privacyUrl);
#if defined(PAG_MACOS)
  context->setContextProperty("DefaultFontFamily", QString("PingFang SC"));
#elif defined(PAG_WINDOWS)
  context->setContextProperty("DefaultFontFamily", QString("Microsoft YaHei"));
#endif

#if defined(QT_DEBUG)
#if defined(PAG_MACOS)
  QString qmlPath = QCoreApplication::applicationDirPath() + "/../../../../../../src/resources/qml/MainWindow.qml";
#elif defined(PAG_WINDOWS)
  QString qmlPath = QCoreApplication::applicationDirPath() + "/../../../src/resources/qml/MainWindow.qml";
#endif
  qmlEngine->load(QUrl::fromLocalFile(QFileInfo(qmlPath).absoluteFilePath()));
#else
  qmlEngine->load(QUrl(QStringLiteral("qrc:/qml/MainWindow.qml")));
#endif
  quickWindow = dynamic_cast<QQuickWindow* >(qmlEngine->rootObjects().at(0));
  quickWindow->setPersistentGraphics(true);
  quickWindow->setPersistentSceneGraph(true);
  quickWindow->setTextRenderType(QQuickWindow::TextRenderType::NativeTextRendering);

  viewWindow = quickWindow->findChild<PAGViewWindow*>("pagViewer");
  runTimeModelManager = quickWindow->findChild<PAGRunTimeModelManager*>("runTimeModelManager");

  connect(viewWindow, &PAGViewWindow::fileChanged, fileInfoModel, &PAGFileInfoModel::updateFileInfo);
  connect(viewWindow, &PAGViewWindow::fileChanged, runTimeModelManager, &PAGRunTimeModelManager::resetFile);
  connect(viewWindow, &PAGViewWindow::frameMetricsReady, runTimeModelManager, &PAGRunTimeModelManager::updateDisplayData);
  connect(viewWindow, &PAGViewWindow::showVideoFramesChanged, this, &PAGWindow::onShowVideoFramesChanged);
  connect(quickWindow, SIGNAL(closing(QQuickCloseEvent*)), this, SLOT(onPAGViewerDestroyed()), Qt::QueuedConnection);
  // TODO
}

auto PAGWindow::getFilePath() -> QString {
  return viewWindow->getFilePath();
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
    viewWindow->setFile(path);
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
    iter->viewWindow->setShowVideoFrames(show);
  }
}

auto PAGWindow::openProject(const QString& path) -> void {
  Q_EMIT openPAGFile(QString(""));
}

QList<PAGWindow* > PAGWindow::AllWindows;