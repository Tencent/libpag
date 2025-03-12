#ifndef RENDERING_RENDERTHREAD_H_
#define RENDERING_RENDERTHREAD_H_

#include <QThread>
#include <QtGui/QOpenGLContext>
#include <pag/pag.h>
#include <platform/qt/GPUDrawable.h>

class PAGViewWindow;

class PAGRenderThread : public QThread {
  Q_OBJECT
 public:
  explicit PAGRenderThread(PAGViewWindow* item);
  ~PAGRenderThread() override;

  Q_SIGNAL void textureReady();
  Q_SIGNAL void firstFrameReady();

  Q_SLOT void shutDown();
  Q_SLOT void renderNext();

#if defined(WIN32)
  auto refreshTextCacheOnWindows() -> void;
#endif

  auto setPAGFile(const std::shared_ptr<pag::PAGFile>& file) -> void;
  auto renderBlank() -> void;
  auto initContexts() -> void;
  auto tryToReplaceImage() const -> bool;
  auto resetReplaceImageInfo() const -> void;

 public:
  QOpenGLContext* openGlContext = nullptr;
  std::shared_ptr<pag::GPUDrawable> drawable;

 private:
  bool fileChanged = false;
  bool shouldTriggerFirstFrame = false;
  double pagProgress = 0.0f;
  QSize bufferSize;
  pag::PAGPlayer* player = nullptr;
  PAGViewWindow* pagWindow = nullptr;
  std::shared_ptr<pag::PAGFile> pagFile;
  std::shared_ptr<pag::PAGSurface> pagSurface;
};

#endif // RENDERING_RENDERTHREAD_H_