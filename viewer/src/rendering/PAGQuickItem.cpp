#include "PAGQuickItem.h"
#include <QSettings>
#include <QJsonArray>
#include <QQuickWindow>
#include <QJsonDocument>
#include <QOpenGLTexture>
#include <QtGui/QOpenGLContext>

PAGQuickItem::PAGQuickItem(QQuickItem* parent) : QQuickItem(parent) {
  resizeTimer = new QTimer(this);
  setFlag(ItemHasContents, true);
  // renderThread = new RenderThread(this);
  // audioHandler = std::make_shared<AudioHandler>();s
  connect(resizeTimer, SIGNAL(timeout()), this, SLOT(sizeChangeDelayFinish()));
}

PAGQuickItem::~PAGQuickItem() {
  delete resizeTimer;
}

auto PAGQuickItem::getProgress() const -> double {
  // if (pagFile == nullptr) {
  if (true) {
    return 0.0;
  }
  return progress;
}

auto PAGQuickItem::getFilePath() -> QString {
  return QString::fromLocal8Bit(filePath.data());
}

auto PAGQuickItem::getDuration() -> double {
  // if (pagFile == nullptr) {
  if (true) {
    return 1.0;
  }
  return duration / 1000.0;
}

auto PAGQuickItem::getPAGWidth() -> int {
  return 100;
  // if (pagFile == nullptr) {
  //   return 100;
  // }
  // return pagFile->width();
}

auto PAGQuickItem::getPAGHeight() -> int {
  return 100;
  // if (pagFile == nullptr) {
  //   return 100;
  // }
  // return pagFile->height();
}

auto PAGQuickItem::getTextCount() const -> int {
  return textCount;
}

auto PAGQuickItem::getIsPlaying() const -> bool {
  return isPlaying;
}

auto PAGQuickItem::getImageCount() const -> int {
  return imageCount;
}

auto PAGQuickItem::getTotalFrame() -> int {
  return 0;
  // if (pagFile == nullptr) {
  //   return 0;
  // }
  // int totalFrames = static_cast<int>((getDuration() * pagFile->frameRate() + 500) / 1000);
  // if (totalFrames < 1) {
  //   totalFrames = 1;
  // }
  // return totalFrames;
}

auto PAGQuickItem::getCurrentFrame() -> int {
  int totalFrames = getTotalFrame();
  return static_cast<int>(getProgress() * (totalFrames - 1) + 0.5);
}

auto PAGQuickItem::getPreferredSize() -> QSizeF {
  return {200, 200};
  // if (pagFile == nullptr) {
  //   return {200, 200};
  // }
  //
  // auto quickWindow = window();
  // int px = quickWindow->x();
  // int py = quickWindow->y();
  // int sw = quickWindow->width();
  // int sh = quickWindow->height();
  // int minwidth = quickWindow->minimumWidth();
  // qreal screenRatio = quickWindow->devicePixelRatio();
  // qreal scaleRatio = quickWindow->devicePixelRatio();
  // int pagWidth = getPAGWidth();
  // int pagHeight = getPAGHeight();
  // auto screen = quickWindow->screen();
  // QSize screenSize = screen->availableVirtualSize();
  // while ((pagHeight / scaleRatio) > (screenSize.height() * 0.9)) {
  //   scaleRatio *= 1.2;
  // }
  // while ((pagWidth / scaleRatio) > screenSize.width()) {
  //   scaleRatio *= 1.2;
  // }
  //
  // auto height = pagHeight / scaleRatio;
  // auto width = pagWidth / scaleRatio;
  // if ((height < quickWindow->minimumHeight()) && (width < quickWindow->minimumWidth())) {
  //   if (height > width) {
  //     height = quickWindow->minimumHeight();
  //     scaleRatio = pagHeight / height;
  //     width = pagWidth / scaleRatio;
  //   } else {
  //     width = quickWindow->minimumWidth();
  //     scaleRatio = pagWidth / width;
  //     height = pagHeight / scaleRatio;
  //   }
  // }
  // qDebug() << "get preferredSize: (" << width << "," << height << ")";
  // return {width, height};
}

auto PAGQuickItem::getShowVideoFrames() const -> bool {
  return showVideoFrames;
}

auto PAGQuickItem::getBackgroundColor() -> QColor {
  return QColor::fromRgb(0, 0, 0);
  // if (pagFile == nullptr) {
  //   return QColor::fromRgb(0, 0, 0);
  // }
  // auto color = pagFile->getFile()->getRootLayer()->composition->backgroundColor;
  // return QColor::fromRgb((int32_t)color.red, (int32_t)color.green, (int32_t)color.blue);
}

auto PAGQuickItem::setProgress(double progress, bool isAudioSeek) -> void {
  // if (pagFile == nullptr) {
  //   return;
  // }
  // if (this->progress == progress) {
  //   return;
  // }
  // if (!isPlaying) {
  //   isAudioSeek = true;
  // }
  // if ((audioHandler != nullptr) && audioHandler->isNeedSeekFix()) {
  //   isAudioSeek = true;
  //   audioHandler->markSeekFix();
  //   auto markPlayingStatus = isPlaying;
  //   firstFrame();
  //   setIsPlaying(markPlayingStatus);
  // }
  // if ((audioHandler != nullptr) && isAudioSeek) {
  //   audioHandler->onProgressChange(progress);
  // }
  // this->progress = progress;
  // Q_EMIT progressChanged(this->progress);
  // update();
}

auto PAGQuickItem::setIsPlaying(bool isPlaying) -> void {
  // if (pagFile == nullptr) {
  //   return;
  // }
  // if (this->isPlaying == isPlaying) {
  //   return;
  // }
  // this->isPlaying = isPlaying;
  // if (this->isPlaying) {
  //   lastTime = GetTimer();
  // }
  // Q_EMIT isPlayingChanged(this->isPlaying);
  // audioHandler->onPlayingChange(isPlaying);
  // update();
}

auto PAGQuickItem::setShowVideoFrames(bool show) -> void {
  if (showVideoFrames == show) {
    return;
  }
  showVideoFrames = show;
  Q_EMIT showVideoFramesChanged(show);
  fileInfoChange();
}

auto PAGQuickItem::ready() -> void {
  // renderThread->moveToThread(renderThread);
  // renderThread->initContexts();
  //
  // connect(window(), SIGNAL(closing(QQuickCloseEvent*)), audioHandler.get(), SLOT(shutDown()),
  //         Qt::QueuedConnection);
  // connect(window(), SIGNAL(closing(QQuickCloseEvent*)), renderThread, SLOT(shutDown()),
  //         Qt::QueuedConnection);
  //
  // renderThread->start();
  // update();
}

auto PAGQuickItem::firstFrameReady() -> void {
  // if (audioHandler != nullptr) {
  //   audioHandler->initContexts();
  // }
  // setVisible(true);
  // setIsPlaying(true);
  // setProgress(0, true);
}

auto PAGQuickItem::sizeChangeDelayFinish() -> void {
  resizeTimer->stop();
  sizeChanged = true;
  update();
}

auto PAGQuickItem::setFile(QString path) -> void {
  return;
#if 0
  auto strPath = std::string(path.toLocal8Bit());

  if (path.startsWith("file://")) {
    strPath = std::string(QUrl(path).toLocalFile().toLocal8Bit());
  }

  filePath = strPath;

  auto data = pag::ByteData::FromPath(filePath);
  if (data == nullptr) {
    return;
  }
  auto encryptedInfo = pag::EnterpriseCodec::GetEncryptedInfo(filePath);
  auto encryptedType = encryptedInfo.type;
  if (encryptedType == pag::EncryptedType::Password) {
    pagFile = pag::PAGFile::Load(data->data(), data->length(), "", filePassword);

    if (pagFile != nullptr) {
      QSettings settings;
      auto value = settings.value("DECRYPTION_PASSWORD_SETTINGS_KEY");
      if (!value.isNull()) {
        QString jsonArrayString = value.toString();
        auto doc = QJsonDocument::fromJson(jsonArrayString.toUtf8());
        auto jsonArray = doc.array();
        for (auto&& password : jsonArray) {
          pagFile = pag::PAGFile::Load(data->data(), data->length(), filePath,
                                       password.toString().toStdString());
          if (pagFile != nullptr) {
            filePassword = password.toString().toStdString();
            break;
          }
        }
      }
    }

    if (pagFile == nullptr) {
      Q_EMIT fileNeedPassword();
      return;
    }
  } else if (encryptedType == pag::EncryptedType::License) {
    pagFile = pag::PAGFile::Load(data->data(), data->length());
    if (!pagFile) {
      Q_EMIT fileNeedLicense(QString::fromStdString(encryptedInfo.name),
                             QString::fromStdString(encryptedInfo.customInfo));
      return;
    }
  } else {
    pagFile = pag::PAGFile::Load(data->data(), data->length());
  }
  if (pagFile == nullptr) {
    return;
  }

  setVisible(false);
  setSize(getPreferredSize());
  sizeChanged = true;
  renderThread->setPAGFile(pagFile);
  audioHandler->setPAGFile(pagFile);
  Q_EMIT fileChanged(pagFile, strPath);
  setIsPlaying(false);
  setProgress(0);

  duration = pagFile->duration();
  textCount = pagFile->getEditableIndices(pag::LayerType::Text).size();
  imageCount = pagFile->getEditableIndices(pag::LayerType::Image).size();

  Q_EMIT textCountChanged(textCount);
  Q_EMIT imageCountChanged(imageCount);

  reportForOpenPAG(data->length());
#endif
}

auto PAGQuickItem::firstFrame() -> void {
  // if (pagFile == nullptr) {
  //   return;
  // }
  // setIsPlaying(false);
  // setProgress(0, true);
}

auto PAGQuickItem::lastFrame() -> void {
  // if (pagFile == nullptr) {
  //   return;
  // }
  // setIsPlaying(false);
  // setProgress(1, true);
}

auto PAGQuickItem::nextFrame() -> void {
  return;
  // if (pagFile == nullptr) {
  //   return;
  // }
  // setIsPlaying(false);
  // auto progress = this->progress + 1.0 / (pagFile->frameRate() * pagFile->duration() / 1000000);
  // if (progress > 1) {
  //   progress = 0;
  // }
  // setProgress(progress, true);
}

auto PAGQuickItem::previousFrame() -> void {
  // if (pagFile == nullptr) {
  //   return;
  // }
  // setIsPlaying(false);
  // auto progress = this->progress - 1.0 / (pagFile->frameRate() * pagFile->duration() / 1000000);
  // if (progress < 0) {
  //   progress = 1;
  // }
  // setProgress(progress, true);
}

auto PAGQuickItem::setPassword(const QString& password) -> void {
  filePassword = password.toStdString();
}

auto PAGQuickItem::checkPassword(const QString& filePath, const QString& password) -> bool {
  return false;
  // auto strPath = std::string(filePath.toLocal8Bit());
  // auto data = pag::ByteData::FromPath(strPath);
  // if (data == nullptr) {
  //   return false;
  // }
  // pagFile = pag::PAGFile::Load(data->data(), data->length(), strPath, password.toStdString());
  // if (pagFile == nullptr) {
  //   return false;
  // }
  // return true;
}

auto PAGQuickItem::fileInfoChange() -> void {
  update();
}

auto PAGQuickItem::onReplaceImage(int index, const QString& imgPath) -> void {
  // if (pagFile == nullptr) {
  //   return;
  // }
  // if (index < 0) {
  //   return;
  // }
  // replaceImageIndex = index;
  // replaceImagePath = std::string(imgPath.toUtf8());
}

auto PAGQuickItem::replaceImageAtPoint(const QString& path, float x, float y) -> void {
  // if (pagFile == nullptr) {
  //   return;
  // }
  // if (pagFile->numImages()) {
  //   replaceImagePath = std::string(path.toUtf8());
  //   if (path.startsWith("file://")) {
  //     replaceImagePath = std::string(QUrl(path).toLocalFile().toUtf8());
  //   }
  //   if ((x <= 0) || (y <= 0)) {
  //     replaceImageIndex = 0;
  //   } else {
  //     replaceImageAtX = x;
  //     replaceImageAtY = y;
  //   }
  // }
}

auto PAGQuickItem::getFboSize() const -> QSize {
  auto size = this->size().toSize();
  if (window() != nullptr) {
    size = size * window()->devicePixelRatio();
  }
  return size;
}

auto PAGQuickItem::getFramebufferId() const -> GLint {
  return bufferId;
}

auto PAGQuickItem::geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) -> void {
  if (newGeometry == oldGeometry) {
    return;
  }
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  resizeTimer->start(400);
}

auto PAGQuickItem::getRenderThread() -> QThread* {
  return nullptr;
  // return renderThread;
}

auto PAGQuickItem::updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) -> QSGNode* {
  return nullptr;
#if 0
  auto* node = static_cast<TextureNode*>(oldNode);

  if (renderThread->openGlContext == nullptr) {
    QSGRendererInterface *rendererInterface = window()->rendererInterface();
    if (rendererInterface == nullptr) {
      qDebug() << "Error: Get null QSGRenderInterface" << Qt::endl;
      return nullptr;
    }

    // 获取 OpenGL 上下文
    auto *current = static_cast<QOpenGLContext *>(rendererInterface->getResource(window(), QSGRendererInterface::OpenGLContextResource));
    if (current == nullptr) {
      qDebug() << "Error: Get null QOpenGLContext" << Qt::endl;
      return nullptr;
    }

    renderThread->openGlContext = new QOpenGLContext();
    renderThread->openGlContext->setFormat(current->format());
    renderThread->openGlContext->setShareContext(current);
    renderThread->openGlContext->create();
    renderThread->openGlContext->moveToThread(renderThread);

    current->makeCurrent(window());
    QMetaObject::invokeMethod(this, "ready");
    return nullptr;
  }

  if (!node) {
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
    connect(renderThread, &RenderThread::textureReady, this, &PAGQuickItem::update, Qt::QueuedConnection);
    connect(renderThread, &RenderThread::firstFrameReady, this, &PAGQuickItem::firstFrameReady, Qt::QueuedConnection);
    connect(window(), &QQuickWindow::afterRendering, renderThread, &RenderThread::renderNext, Qt::QueuedConnection);

    // Get the production of FBO textures started..
    QMetaObject::invokeMethod(renderThread, "renderNext", Qt::QueuedConnection);

    lastDevicePixelRatio = window()->devicePixelRatio();
  }
  if (!renderThread->drawable) {
    printf("m_renderThread->drawable null\n");
    return nullptr;
  }
  auto texture = renderThread->drawable->getTexture();
  if (texture) {
    node->setTexture(texture);
    node->markDirty(QSGNode::DirtyMaterial);
  }
  node->setRect(boundingRect());
  if (lastDevicePixelRatio != window()->devicePixelRatio()) {
    sizeChangeDelayFinish();
    lastDevicePixelRatio = window()->devicePixelRatio();
  }

  auto time = GetTimer() - lastTime;
  lastTime = GetTimer();

  if (isPlaying) {
    auto progress = progress + (time * 1.0 / duration);
    bool isAudioSeek = false;
    if (progress > 1) {
      progress = 0;
      isAudioSeek = true;
    }
    setProgress(progress, isAudioSeek);
  }
  return node;
#endif
}

auto PAGQuickItem::setFramebufferId(GLint id) -> void {
  bufferId = id;
}

auto PAGQuickItem::reportForOpenPAG(size_t data_length) -> void {
  //TODO
}