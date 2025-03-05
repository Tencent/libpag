#include "PAGViewWindow.h"
#include <chrono>
#include <QSettings>
#include <QJsonArray>
#include <QQuickWindow>
#include <QJsonDocument>
#include <QOpenGLTexture>
#include <QtGui/QOpenGLContext>
#include <pag/file.h>
#include "report/PAGReport.h"

int64_t GetPassTime() {
  static auto start_time = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto us = std::chrono::duration_cast<std::chrono::microseconds>(now - start_time);
  return static_cast<int64_t>(us.count());
}

PAGViewWindow::PAGViewWindow(QQuickItem* parent) : QQuickItem(parent) {
  resizeTimer = new QTimer(this);
  setFlag(ItemHasContents, true);
  renderThread = new PAGRenderThread(this);
  connect(resizeTimer, SIGNAL(timeout()), this, SLOT(sizeChangeDelayFinish()));
}

PAGViewWindow::~PAGViewWindow() {
  delete resizeTimer;
}

auto PAGViewWindow::getProgress() const -> double {
  if (pagFile == nullptr) {
    return 0.0;
  }
  return progress;
}

auto PAGViewWindow::getFilePath() -> QString {
  return QString::fromLocal8Bit(filePath.data());
}

auto PAGViewWindow::getDuration() const -> double {
  if (pagFile == nullptr) {
    return 1.0;
  }
  return static_cast<double>(duration) / 1000.0;
}

auto PAGViewWindow::getPAGWidth() const -> int {
  if (pagFile == nullptr) {
    return 100;
  }
  return pagFile->width();
}

auto PAGViewWindow::getPAGHeight() const -> int {
  if (pagFile == nullptr) {
    return 100;
  }
  return pagFile->height();
}

auto PAGViewWindow::getTextCount() const -> int {
  return textCount;
}

auto PAGViewWindow::getIsPlaying() const -> bool {
  return isPlaying;
}

auto PAGViewWindow::getImageCount() const -> int {
  return imageCount;
}

auto PAGViewWindow::getTotalFrame() const -> int {
  if (pagFile == nullptr) {
    return 0;
  }
  int totalFrames = static_cast<int>((getDuration() * pagFile->frameRate() + 500) / 1000);
  if (totalFrames < 1) {
    totalFrames = 1;
  }
  return totalFrames;
}

auto PAGViewWindow::getCurrentFrame() const -> int {
  int totalFrames = getTotalFrame();
  return static_cast<int>(getProgress() * (totalFrames - 1) + 0.5);
}

auto PAGViewWindow::getPreferredSize() const -> QSizeF {
  if (pagFile == nullptr) {
    return {200, 200};
  }

  auto quickWindow = window();
  qreal scaleRatio = quickWindow->devicePixelRatio();
  int pagWidth = getPAGWidth();
  int pagHeight = getPAGHeight();
  auto screen = quickWindow->screen();
  QSize screenSize = screen->availableVirtualSize();
  while ((pagHeight / scaleRatio) > (screenSize.height() * 0.9)) {
    scaleRatio *= 1.2;
  }
  while ((pagWidth / scaleRatio) > screenSize.width()) {
    scaleRatio *= 1.2;
  }

  auto height = pagHeight / scaleRatio;
  auto width = pagWidth / scaleRatio;
  if ((height < quickWindow->minimumHeight()) && (width < quickWindow->minimumWidth())) {
    if (height > width) {
      height = quickWindow->minimumHeight();
      scaleRatio = pagHeight / height;
      width = pagWidth / scaleRatio;
    } else {
      width = quickWindow->minimumWidth();
      scaleRatio = pagWidth / width;
      height = pagHeight / scaleRatio;
    }
  }

  return {width, height};
}

auto PAGViewWindow::getShowVideoFrames() const -> bool {
  return showVideoFrames;
}

auto PAGViewWindow::getBackgroundColor() const -> QColor {
  if (pagFile == nullptr) {
    return QColor::fromRgb(0, 0, 0);
  }

  auto color = pagFile->getFile()->getRootLayer()->composition->backgroundColor;
  return QColor::fromRgb((int32_t)color.red, (int32_t)color.green, (int32_t)color.blue);
}

auto PAGViewWindow::setProgress(double progress, bool isAudioSeek) -> void {
  if (pagFile == nullptr) {
    return;
  }
  if (this->progress == progress) {
    return;
  }
  if (!isPlaying) {
    isAudioSeek = true;
  }
  this->progress = progress;
  Q_EMIT progressChanged(this->progress);
  update();
}

auto PAGViewWindow::setIsPlaying(bool isPlaying) -> void {
  if (pagFile == nullptr) {
    return;
  }
  if (this->isPlaying == isPlaying) {
    return;
  }
  this->isPlaying = isPlaying;
  if (this->isPlaying) {
    lastTime = GetPassTime();
  }
  Q_EMIT isPlayingChanged(this->isPlaying);
  update();
}

auto PAGViewWindow::setShowVideoFrames(bool show) -> void {
  if (showVideoFrames == show) {
    return;
  }
  showVideoFrames = show;
  Q_EMIT showVideoFramesChanged(show);
  fileInfoChange();
}

auto PAGViewWindow::ready() -> void {
  renderThread->moveToThread(renderThread);
  renderThread->initContexts();
  connect(window(), SIGNAL(closing(QQuickCloseEvent*)), renderThread, SLOT(shutDown()),
          Qt::QueuedConnection);
  renderThread->start();
  update();
}

auto PAGViewWindow::firstFrameReady() -> void {
  setVisible(true);
  setIsPlaying(true);
  setProgress(0, true);
}

auto PAGViewWindow::sizeChangeDelayFinish() -> void {
  if (resizeTimer != nullptr) {
    resizeTimer->stop();
  }
  sizeChanged = true;
  update();
}

auto PAGViewWindow::setFile(const QString& path) -> void {
  auto strPath = std::string(path.toLocal8Bit());
  if (path.startsWith("file://")) {
    strPath = std::string(QUrl(path).toLocalFile().toLocal8Bit());
  }
  filePath = strPath;

  auto data = pag::ByteData::FromPath(filePath);
  if (data == nullptr) {
    return;
  }

  pagFile = pag::PAGFile::Load(data->data(), data->length());
  if (pagFile == nullptr) {
    return;
  }

  setProgress(0);
  setVisible(false);
  setIsPlaying(false);

  setSize(getPreferredSize());
  sizeChanged = true;
  renderThread->setPAGFile(pagFile);

  duration = pagFile->duration();
  textCount = pagFile->getEditableIndices(pag::LayerType::Text).size();
  imageCount = pagFile->getEditableIndices(pag::LayerType::Image).size();
  progressPerFrame = 1.0 / (pagFile->frameRate() * pagFile->duration() / 1000000);
  reportForOpenPAG(data->length());

  Q_EMIT fileChanged(pagFile, strPath);
  Q_EMIT textCountChanged(textCount);
  Q_EMIT imageCountChanged(imageCount);
}

auto PAGViewWindow::firstFrame() -> void {
  if (pagFile == nullptr) {
    return;
  }
  setIsPlaying(false);
  setProgress(0, true);
}

auto PAGViewWindow::lastFrame() -> void {
  if (pagFile == nullptr) {
    return;
  }
  setIsPlaying(false);
  setProgress(1, true);
}

auto PAGViewWindow::nextFrame() -> void {
  if (pagFile == nullptr) {
    return;
  }
  setIsPlaying(false);
  auto progress = this->progress + progressPerFrame;
  if (progress > 1) {
    progress = 0;
  }
  setProgress(progress, true);
}

auto PAGViewWindow::previousFrame() -> void {
  if (pagFile == nullptr) {
    return;
  }
  setIsPlaying(false);
  auto progress = this->progress - progressPerFrame;
  if (progress < 0) {
    progress = 1;
  }
  setProgress(progress, true);
}

auto PAGViewWindow::fileInfoChange() -> void {
  update();
}

auto PAGViewWindow::onReplaceImage(int index, const QString& imgPath) -> void {
  if (pagFile == nullptr) {
    return;
  }
  if (index < 0) {
    return;
  }
  replaceImageIndex = index;
  replaceImagePath = std::string(imgPath.toUtf8());
}

auto PAGViewWindow::replaceImageAtPoint(const QString& path, float x, float y) -> void {
  if (pagFile == nullptr) {
    return;
  }
  if (pagFile->numImages() > 0) {
    replaceImagePath = std::string(path.toUtf8());
    if (path.startsWith("file://")) {
      replaceImagePath = std::string(QUrl(path).toLocalFile().toUtf8());
    }
    if ((x <= 0) || (y <= 0)) {
      replaceImageIndex = 0;
    } else {
      replaceImageAtX = x;
      replaceImageAtY = y;
    }
  }
}

auto PAGViewWindow::getFboSize() const -> QSize {
  auto size = this->size().toSize();
  if (window() != nullptr) {
    size = size * window()->devicePixelRatio();
  }
  return size;
}

auto PAGViewWindow::getFramebufferId() const -> GLint {
  return bufferId;
}

auto PAGViewWindow::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) -> void {
  if (newGeometry == oldGeometry) {
    return;
  }
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  resizeTimer->start(400);
}

auto PAGViewWindow::getRenderThread() const -> QThread* {
  return renderThread;
}

auto PAGViewWindow::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) -> QSGNode* {
  auto* node = dynamic_cast<TextureNode*>(oldNode);

  if (renderThread->openGlContext == nullptr) {
    QSGRendererInterface *rendererInterface = window()->rendererInterface();
    if (rendererInterface == nullptr) {
      qDebug() << "Error: Get null QSGRenderInterface";
      return nullptr;
    }

    auto *currentContext = static_cast<QOpenGLContext *>(rendererInterface->getResource(window(), QSGRendererInterface::OpenGLContextResource));
    if (currentContext == nullptr) {
      qDebug() << "Error: Get null QOpenGLContext" << Qt::endl;
      return nullptr;
    }

    renderThread->openGlContext = new QOpenGLContext();
    renderThread->openGlContext->setFormat(currentContext->format());
    renderThread->openGlContext->setShareContext(currentContext);
    renderThread->openGlContext->create();
    renderThread->openGlContext->moveToThread(renderThread);

    currentContext->makeCurrent(window());
    QMetaObject::invokeMethod(this, "ready");
    return nullptr;
  }

  if (node == nullptr) {
    node = new TextureNode(window());
    /* Set up connections to get the production of FBO textures in sync with vsync on the
     * rendering thread.
     *
     * When a new texture is ready on the rendering thread, we use a direct connection to
     * the texture node to let it know a new texture can be used. The node will then
     * emit pendingNewTexture which we bind to QQuickWindow::update to schedule a redraw.
     *
     * When the scene graph starts rendering the next frame, the prepareNode() function
     * is used to update the node with the new texture. Once it completes, it emits
     * textureInUse() which we connect to the FBO rendering thread's renderNext() to have
     * it start producing content into its current "back buffer".
     *
     * This FBO rendering pipeline is throttled by vsync on the scene graph rendering thread.
     */
    connect(renderThread, &PAGRenderThread::textureReady, this, &PAGViewWindow::update, Qt::QueuedConnection);
    connect(renderThread, &PAGRenderThread::firstFrameReady, this, &PAGViewWindow::firstFrameReady, Qt::QueuedConnection);
    connect(window(), &QQuickWindow::afterRendering, renderThread, &PAGRenderThread::renderNext, Qt::QueuedConnection);

    QMetaObject::invokeMethod(renderThread, "renderNext", Qt::QueuedConnection);

    lastDevicePixelRatio = window()->devicePixelRatio();
  }

  if (renderThread->drawable == nullptr) {
    qDebug() << "GPUDrawable is null";
    return nullptr;
  }

  if (renderThread->drawable->getTexture() != nullptr) {
    node->setTexture(renderThread->drawable->getTexture());
    node->markDirty(QSGNode::DirtyMaterial);
  }

  node->setRect(boundingRect());
  if (lastDevicePixelRatio != window()->devicePixelRatio()) {
    sizeChangeDelayFinish();
    lastDevicePixelRatio = window()->devicePixelRatio();
  }

  auto time = GetPassTime() - lastTime;
  lastTime = GetPassTime();

  if (isPlaying) {
    auto progress = this->progress + (static_cast<double>(time) / duration);
    bool isAudioSeek = false;
    if (progress > 1) {
      progress = 0;
      isAudioSeek = true;
    }
    setProgress(progress, isAudioSeek);
  }
  return node;
}

auto PAGViewWindow::setFramebufferId(GLint id) -> void {
  bufferId = id;
}

auto PAGViewWindow::reportForOpenPAG(size_t data_length) const -> void {
  PAGReport::getInstance()->setEvent("OPEN_PAG");
  PAGReport::getInstance()->addParam("FileSize", std::to_string(data_length));
  PAGReport::getInstance()->addParam("PAGLayerCount", std::to_string(pagFile->numChildren()));
  PAGReport::getInstance()->addParam("VideoCompositionCount", std::to_string(pagFile->numVideos()));
  PAGReport::getInstance()->addParam("PAGTextLayerCount", std::to_string(pagFile->numTexts()));
  PAGReport::getInstance()->addParam("PAGImageLayerCount", std::to_string(pagFile->numImages()));
  PAGReport::getInstance()->report();
}