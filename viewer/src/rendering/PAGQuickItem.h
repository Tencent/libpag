#ifndef RENDERING_PAGQUICKITEM_H_
#define RENDERING_PAGQUICKITEM_H_

#include <QTimer>
#include <QOpenGLShaderProgram>
#include <QtQuick/QQuickPaintedItem>
#include "PAGQuickItemTypes.h"

class PAGQuickItem : public QQuickItem, public PAGQuickItemProtocol
{
  Q_OBJECT
 public:
  explicit PAGQuickItem(QQuickItem* parent = nullptr);
  ~PAGQuickItem() override;

  Q_PROPERTY(int duration           READ getDuration)
  Q_PROPERTY(int pagWidth           READ getPAGWidth)
  Q_PROPERTY(int pagHeight          READ getPAGHeight)
  Q_PROPERTY(QString filePath       READ getFilePath)
  Q_PROPERTY(int currentFrame       READ getCurrentFrame)
  Q_PROPERTY(QSizeF preferredSize   READ getPreferredSize)
  Q_PROPERTY(QColor backgroundColor READ getBackgroundColor)
  Q_PROPERTY(int totalFrame         READ getTotalFrame)
  Q_PROPERTY(int textCount          READ getTextCount                                   NOTIFY textCountChanged)
  Q_PROPERTY(int imageCount         READ getImageCount                                  NOTIFY imageCountChanged)
  Q_PROPERTY(double showVideoFrames READ getShowVideoFrames   WRITE setShowVideoFrames)
  Q_PROPERTY(double progress        READ getProgress          WRITE setProgress         NOTIFY progressChanged)
  Q_PROPERTY(double isPlaying       READ getIsPlaying         WRITE setIsPlaying        NOTIFY isPlayingChanged)

  auto getProgress() const -> double;
  auto getFilePath() -> QString;
  auto getDuration() -> double;
  auto getPAGWidth() -> int;
  auto getPAGHeight() -> int;
  auto getTextCount() const -> int;
  auto getIsPlaying() const -> bool;
  auto getImageCount() const -> int;
  auto getTotalFrame() -> int;
  auto getCurrentFrame() -> int;
  auto getPreferredSize() -> QSizeF;
  auto getShowVideoFrames() const -> bool;
  auto getBackgroundColor() -> QColor;

  auto setProgress(double progress, bool isAudioSeek = false) -> void;
  auto setIsPlaying(bool isPlaying) -> void;
  auto setShowVideoFrames(bool show) -> void;

  // TODO
  // Q_SIGNAL void fileChanged(std::shared_ptr<pag::PAGFile> pagFile, std::string filePath);
  Q_SIGNAL void fileChanged(std::string filePath);
  Q_SIGNAL void progressChanged(double progress);
  Q_SIGNAL void isPlayingChanged(bool isPlaying);
  Q_SIGNAL void textCountChanged(int count);
  Q_SIGNAL void imageCountChanged(int count);
  Q_SIGNAL void frameMetricsReady(int frame, int renderingTime, int presentingTime, int imageDecodingTime);
  Q_SIGNAL void updateImageModelAt(int index, std::string path);
  Q_SIGNAL void showVideoFramesChanged(bool show);

  Q_SLOT void ready();
  Q_SLOT void firstFrameReady();
  Q_SLOT void sizeChangeDelayFinish();

  Q_INVOKABLE void setFile(QString path);
  Q_INVOKABLE void firstFrame();
  Q_INVOKABLE void lastFrame();
  Q_INVOKABLE void nextFrame();
  Q_INVOKABLE void setPassword(const QString& password);
  Q_INVOKABLE bool checkPassword(const QString& filePath, const QString& password);
  Q_INVOKABLE void previousFrame();
  Q_INVOKABLE void fileInfoChange();
  Q_INVOKABLE void onReplaceImage(int index, const QString& imgPath);
  Q_INVOKABLE void replaceImageAtPoint(const QString& path, float x, float y);

  auto getFboSize() const -> QSize override;
  auto getFramebufferId() const -> GLint override;
  auto geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) -> void override;
  auto getRenderThread() -> QThread*;
  auto updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) -> QSGNode* override;
  auto setFramebufferId(GLint id) -> void;
  auto reportForOpenPAG(size_t data_length) -> void;

public:
  bool sizeChanged = false;
  int replaceImageIndex = -1;
  float replaceImageAtX = -1;
  float replaceImageAtY = -1;
  std::string replaceImagePath;
  QTimer* resizeTimer = nullptr;

 private:
  bool isPlaying = false;
  bool showVideoFrames = true;
  int textCount = 0;
  int imageCount = 0;
  GLint bufferId = 0;
  double progress = 0.0;
  int64_t lastTime = 0;
  int64_t duration = 1;
  qreal lastDevicePixelRatio = 1;
  std::string filePath;
  std::string filePassword;
  // TODO
  // RenderThread* renderThread = nullptr;
  // std::shared_ptr<AudioHandler> audioHandler = nullptr;
  // std::shared_ptr<pag::PAGFile> pagFile = nullptr;
};

#endif // RENDERING_PAGQUICKITEM_H_