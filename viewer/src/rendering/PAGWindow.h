#ifndef RENDERING_PAGWINDOW_H_
#define RENDERING_PAGWINDOW_H_

#include <QOBject>
#include <QQuickWindow>
#include <QQmlApplicationEngine>
#include "rendering/PAGViewWindow.h"
#include "rendering/PAGWindowHelper.h"
#include "translate/PAGLanguageModel.h"

class PAGWindow : public QObject {
  Q_OBJECT
 public:
  explicit PAGWindow(QObject* parent = nullptr);
  ~PAGWindow() override;

  auto Open() -> void;
  auto getEngine() -> QQmlApplicationEngine*;
  auto getFilePath() -> QString;
  
  Q_SLOT void close();
  Q_SLOT void openFile(const QString& path);
  Q_SLOT void onPAGViewerDestroyed();
  Q_SLOT void onShowVideoFramesChanged(bool show);

  Q_SIGNAL void openPAGFile(QString path);
  Q_SIGNAL void destroyWindow(PAGWindow* window);

  Q_INVOKABLE void openProject(const QString& path);

  static QList<PAGWindow* > AllWindows;

 private:
  QString filePath = "";
  QQuickWindow* quickWindow = nullptr;
  QQmlApplicationEngine* qmlEngine = nullptr;
  // ImageLayerModel *imageLayerModel =nullptr;
  // FileInfoModel *infoModel = nullptr;
  // TextLayerModel *textLayerModel = nullptr;
  // CheckUpdateModel *checkUpdateModel = nullptr;
  PAGViewWindow *viewWindow = nullptr;
  // RunTimeModel *runTimeModel = nullptr;
  // FileTaskFactory *taskFactory = nullptr;
  // QQuickWindow *window = nullptr;
  // PAGImageProvider *pagImageProvider = nullptr;
  // TreeModel *pagTreeViewModel = nullptr;
  PAGWindowHelper *windowHelper = nullptr;
  PAGLanguageModel* languageModel = nullptr;
  // PAGEditAttributeModel *pagEditAttributeModel = nullptr;
  // PAGBenchmarkModel *pagBenchmarkModel = nullptr;
  // LicenseManagerModel *licenseManagerModel = nullptr;
  // WatermarkModel *watermarkModel = nullptr;
  // FileSystem *fileSystem = nullptr;
};

#endif // RENDERING_PAGWINDOW_H_