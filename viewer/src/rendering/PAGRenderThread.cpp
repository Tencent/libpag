#include "PAGRenderThread.h"
#include "PAGViewWindow.h"
#include "utils/TimeUtils.h"

PAGRenderThread::PAGRenderThread(PAGViewWindow* item) : pagWindow(item) {

}

PAGRenderThread::~PAGRenderThread() = default;

void PAGRenderThread::shutDown() {
  pagSurface = nullptr;
  openGlContext->doneCurrent();
  delete openGlContext;

  QThread::exit();

  if (QGuiApplication::instance()) {
    moveToThread(QGuiApplication::instance()->thread());
  }
}

void PAGRenderThread::renderNext() {
  auto needRender = pagWindow->getIsPlaying();
  if (pagSurface == nullptr) {
    qDebug() << "PAGSurface is null";
    return;
  }

  if (pagWindow->sizeChanged) {
    needRender = true;
    auto newBufferSize = (pagWindow->size() * pagWindow->window()->devicePixelRatio()).toSize();
    if (newBufferSize.width() <= 0) {
      newBufferSize.setWidth(64);
    }
    if (newBufferSize.height() <= 0) {
      newBufferSize.setHeight(64);
    }
    if (bufferSize != newBufferSize) {
      pagWindow->sizeChanged = false;
      pagSurface->updateSize();
      bufferSize = newBufferSize;
    } else {
      pagWindow->sizeChanged = false;
      pagSurface->updateSize();
    }
  }

  if (fileChanged) {
    player->setComposition(pagFile);
    player->setScaleMode(pag::PAGScaleMode::LetterBox);
    player->setProgress(0);
    fileChanged = false;
    shouldTriggerFirstFrame = true;
    renderBlank();
    return;
  }

  if (player->videoEnabled() != pagWindow->getShowVideoFrames()) {
    player->setVideoEnabled(pagWindow->getShowVideoFrames());
    needRender = true;
  }

  if (pagFile != nullptr) {
    needRender = tryToReplaceImage() || needRender;
    if (pagProgress != pagWindow->getProgress()) {
      player->setProgress(pagWindow->getProgress());
      pagProgress = pagWindow->getProgress();
      needRender = true;
    }
    if (player->getProgress() > 1.0) {
      player->setProgress(0);
      needRender = true;
    }
  } else {
    needRender = false;
    pagSurface->clearAll();
  }

  if (needRender) {
    auto totalFrames = TimeToFrame(pagFile->duration(), pagFile->frameRate()) - 1;
    int frame = static_cast<int>(round(pagFile->getProgress() * static_cast<double>(totalFrames)));
    Q_EMIT pagWindow->frameMetricsReady(frame,static_cast<int>(player->renderingTime()),
      static_cast<int>(player->presentingTime()),
      static_cast<int>(player->imageDecodingTime()));

    if (player->flush()) {
      Q_EMIT textureReady();
    }
  }

  if (shouldTriggerFirstFrame) {
#if defined(PAG_WINDOWS)
    refreshTextCacheOnWindows();
#endif
    Q_EMIT firstFrameReady();
    shouldTriggerFirstFrame = false;
  }
}

#if defined(PAG_WINDOWS)
auto PAGRenderThread::refreshTextCacheOnWindows() -> void {
  for (int i = 0; i < pagFile->numTexts(); i++) {
    auto textData = pagFile->getTextData(i);
    pagFile->replaceText(i, textData);
  }
}
#endif

auto PAGRenderThread::setPAGFile(const std::shared_ptr<pag::PAGFile>& file) -> void {
  if (pagFile != file) {
    pagFile = file;
    fileChanged = true;
  }
}

auto PAGRenderThread::renderBlank() -> void {
  pagSurface->clearAll();
  Q_EMIT textureReady();
}

auto PAGRenderThread::initContexts() -> void {
  if (pagSurface == nullptr) {
    drawable = pag::GPUDrawable::MakeFrom(pagWindow, openGlContext);
    drawable->moveToThread(this);
    pagSurface = pag::PAGSurface::MakeFrom(drawable);
    player = new pag::PAGPlayer();
    player->setSurface(pagSurface);
  }
}

auto PAGRenderThread::tryToReplaceImage() const -> bool {
  if (!pagWindow) {
    return false;
  }
  auto localPath = QString::fromStdString(pagWindow->replaceImagePath).toLocal8Bit().toStdString();
  auto pagImage = localPath.empty() ? nullptr : pag::PAGImage::FromPath(localPath);
  if (!localPath.empty() && pagImage == nullptr) {
    resetReplaceImageInfo();
    return false;
  }

  int index = pagWindow->replaceImageIndex;
  if (pagWindow->replaceImageAtX >= 0) {
    index = 0;
    const auto layers = player->getLayersUnderPoint(pagWindow->replaceImageAtX, pagWindow->replaceImageAtY);
    for (const auto& pagLayer : layers) {
      if (pagLayer->layerType() == pag::LayerType::Image) {
        index = pagLayer->editableIndex();
        break;
      }
    }
  }
  if (index >= 0) {
    pagFile->replaceImage(index, pagImage);
    Q_EMIT pagWindow->updateImageModelAt(index, pagWindow->replaceImagePath);
    resetReplaceImageInfo();
  }
  return true;
}

auto PAGRenderThread::resetReplaceImageInfo() const -> void {
  pagWindow->replaceImageAtX = -1;
  pagWindow->replaceImagePath = "";
  pagWindow->replaceImageIndex = -1;
}