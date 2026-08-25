/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "rendering/pagx/PAGXViewModel.h"
#include <QAbstractTextDocumentLayout>
#include <QDebug>
#include <QFile>
#include <QFontMetrics>
#include <QMetaObject>
#include <QPlainTextDocumentLayout>
#include <QQuickTextDocument>
#include <QQuickWindow>
#include <QTextCursor>
#include <QTime>
#include <QTimer>
#include <QXmlStreamReader>
#include <cmath>
#include "pag/pag.h"
#include "pagx/PAGXImporter.h"

namespace pag {

static void EditorLog(const QString& message) {
  qDebug().noquote() << "[PAGXEditor]" << QTime::currentTime().toString("hh:mm:ss.zzz") << message;
}

// Measures the widest line with the document's font so the QML editor can size its content
// without ever asking QTextDocumentLayout for the ideal width, which would force a full
// layout pass over every block and freeze the UI on large files.
static double MeasureMaxLineWidth(const QTextDocument* document, const QString& text) {
  const QFontMetrics metrics(document->defaultFont());
  double maxWidth = 0;
  qsizetype start = 0;
  while (start < text.size()) {
    auto end = text.indexOf(u'\n', start);
    if (end < 0) {
      end = text.size();
    }
    const auto width =
        metrics.horizontalAdvance(QStringView(text).mid(start, end - start).toString());
    maxWidth = qMax(maxWidth, static_cast<double>(width));
    if (end >= text.size()) {
      break;
    }
    start = end + 1;
  }
  return maxWidth;
}

PAGXViewModel::PAGXViewModel(QObject* parent) : ContentViewModel(parent) {
}

int PAGXViewModel::getWidth() const {
  return pagxWidth;
}

int PAGXViewModel::getHeight() const {
  return pagxHeight;
}

bool PAGXViewModel::hasAnimation() const {
  return totalFrames > 1;
}

bool PAGXViewModel::isPlaying() const {
  return isPlaying_;
}

double PAGXViewModel::getProgress() const {
  return progress;
}

QString PAGXViewModel::getTotalFrame() const {
  return QString::number(totalFrames);
}

QString PAGXViewModel::getCurrentFrame() const {
  auto currentFrame = static_cast<int64_t>(std::floor(progress * static_cast<double>(totalFrames)));
  if (currentFrame >= totalFrames) {
    currentFrame = totalFrames - 1;
  }
  return QString::number(currentFrame);
}

QString PAGXViewModel::getDuration() const {
  if (frameRate <= 0 || totalFrames <= 1) {
    return "0";
  }
  auto durationMs = static_cast<int64_t>(static_cast<double>(totalFrames) / frameRate * 1000.0);
  return QString::number(durationMs);
}

QString PAGXViewModel::getFilePath() const {
  return QString::fromLocal8Bit(currentFilePath.data());
}

QString PAGXViewModel::getDisplayedTime() const {
  auto durationMs = getDuration().toLongLong();
  if (durationMs <= 0) {
    return "0:00";
  }
  auto displayedTimeSeconds =
      static_cast<int64_t>(std::round(progress * static_cast<double>(durationMs) / 1000.0));
  int64_t displayedSeconds = displayedTimeSeconds % 60;
  int64_t displayedMinutes = (displayedTimeSeconds / 60) % 60;
  return QString("%1:%2")
      .arg(displayedMinutes, 2, 10, QChar('0'))
      .arg(displayedSeconds, 2, 10, QChar('0'));
}

QColor PAGXViewModel::getBackgroundColor() const {
  return QColorConstants::White;
}

QSizeF PAGXViewModel::getPreferredSize() const {
  if (window == nullptr || pagxWidth == 0 || pagxHeight == 0) {
    return {0, 0};
  }
  auto screen = window->screen();
  if (screen == nullptr) {
    return {0, 0};
  }
  QSize screenSize = screen->availableVirtualSize();
  qreal maxHeight = screenSize.height() * 0.8;
  qreal minHeight = window->minimumHeight();
  qreal width = 0;
  qreal height = 0;

  if (pagxHeight < minHeight) {
    height = minHeight;
    width = pagxWidth * height / pagxHeight;
  } else {
    height = pagxHeight;
    width = pagxWidth;
  }

  if (height > maxHeight) {
    width = width * maxHeight / height;
    height = maxHeight;
  }

  return {width, height};
}

int PAGXViewModel::getEditableTextLayerCount() const {
  return 0;
}

int PAGXViewModel::getEditableImageLayerCount() const {
  return 0;
}

bool PAGXViewModel::getShowVideoFrames() const {
  return false;
}

ContentViewModel::ContentType PAGXViewModel::getContentType() const {
  return ContentType::PAGX;
}

void PAGXViewModel::setWindow(QQuickWindow* win) {
  window = win;
}

bool PAGXViewModel::takeNeedsRender() {
  return needsRender.exchange(false);
}

void PAGXViewModel::markNeedsRender() {
  needsRender = true;
}

void PAGXViewModel::onViewTransformChanged() {
  markNeedsRender();
}

void PAGXViewModel::updateProgressFromRender(double newProgress, uint64_t generation) {
  QMetaObject::invokeMethod(this, "applyProgressFromRender", Qt::QueuedConnection,
                            Q_ARG(double, newProgress),
                            Q_ARG(quint64, static_cast<quint64>(generation)));
}

void PAGXViewModel::applyProgressFromRender(double newProgress, quint64 generation) {
  // Ignore a progress update from an earlier playback session; content may have been reloaded.
  if (generation != playbackGeneration.load()) {
    return;
  }
  if (std::abs(progress - newProgress) < 1e-9) {
    return;
  }
  progress = newProgress;
  Q_EMIT progressChanged(progress);
}

void PAGXViewModel::notifyPlaybackFinished(uint64_t generation) {
  QMetaObject::invokeMethod(this, "handlePlaybackFinished", Qt::QueuedConnection,
                            Q_ARG(quint64, static_cast<quint64>(generation)));
}

void PAGXViewModel::handlePlaybackFinished(quint64 generation) {
  // Ignore a finish event from an earlier playback: if the user restarted playback after the
  // event was queued, playbackGeneration has advanced and this notification is stale.
  if (generation != playbackGeneration.load()) {
    return;
  }
  setIsPlaying(false);
}

PAGXViewModel::RenderState PAGXViewModel::getRenderState() {
  std::lock_guard<std::mutex> lock(renderMutex);
  return {scene,
          defaultAnimation,
          defaultLoopMode,
          pagxWidth,
          pagxHeight,
          isPlaying_.load(),
          progress,
          pendingSeek.exchange(false),
          playbackGeneration.load(),
          getViewTransform()};
}

bool PAGXViewModel::hasContent() {
  std::lock_guard<std::mutex> lock(renderMutex);
  return scene != nullptr;
}

void PAGXViewModel::setIsPlaying(bool isPlaying) {
  if (!hasAnimation()) {
    isPlaying = false;
  }
  if (isPlaying_ == isPlaying) {
    return;
  }
  if (isPlaying) {
    playbackGeneration++;
  }
  isPlaying_ = isPlaying;
  Q_EMIT isPlayingChanged(isPlaying);
  // Request a render on every transition, not just on play: the render thread needs one flush to
  // observe the state change (re-arm or seek the timeline and update its transition tracker).
  needsRender = true;
  Q_EMIT requestFlush();
}

void PAGXViewModel::setProgress(double newProgress) {
  if (std::abs(progress - newProgress) < 1e-9) {
    return;
  }
  progress = newProgress;
  pendingSeek = true;
  Q_EMIT progressChanged(progress);
  needsRender = true;
  Q_EMIT requestFlush();
}

void PAGXViewModel::setShowVideoFrames(bool) {
}

bool PAGXViewModel::loadFile(const QString& filePath) {
  auto strPath = std::string(filePath.toLocal8Bit());
  if (filePath.startsWith("file://")) {
    strPath = std::string(QUrl(filePath).toLocalFile().toLocal8Bit());
  }
  auto byteData = pag::ByteData::FromPath(strPath);
  if (byteData == nullptr) {
    clearDocumentXml();
    Q_EMIT filePathChanged("");
    return false;
  }

  auto document = pagx::PAGXImporter::FromXML(byteData->data(), byteData->length());
  if (document == nullptr) {
    clearDocumentXml();
    Q_EMIT filePathChanged("");
    return false;
  }
  document->applyLayout();

  auto newScene = pagx::PAGScene::Make(document);
  if (newScene == nullptr) {
    {
      std::lock_guard<std::mutex> lock(renderMutex);
      clearContent();
      // Reset playback state so a prior playing session does not leave the controls stuck; with a
      // null timeline this stops playback, zeroes the frame counters, and bumps the generation.
      updateAnimationState();
    }
    clearDocumentXml();
    Q_EMIT filePathChanged("");
    Q_EMIT pagxDocumentChanged(nullptr);
    emitContentStateReset();
    return false;
  }

  auto xmlString = QString::fromUtf8(reinterpret_cast<const char*>(byteData->data()),
                                     static_cast<qsizetype>(byteData->length()));

  {
    std::lock_guard<std::mutex> lock(renderMutex);
    clearContent();
    currentFilePath = strPath;
    pagxDocument = document;
    scene = newScene;
    resolveDefaultAnimation(document);
    pagxWidth = static_cast<int>(document->width);
    pagxHeight = static_cast<int>(document->height);
    updateAnimationState();
    needsRender = true;
  }
  Q_EMIT filePathChanged(QString::fromLocal8Bit(strPath.data()));
  Q_EMIT widthChanged(pagxWidth);
  Q_EMIT heightChanged(pagxHeight);
  emitContentStateReset();
  Q_EMIT preferredSizeChanged();
  Q_EMIT editableTextLayerCountChanged(0);
  Q_EMIT editableImageLayerCountChanged(0);
  Q_EMIT contentSizeChanged();

  // pagxDocumentChanged is connected with Qt::QueuedConnection, so tree building
  // happens asynchronously and won't block the render.
  Q_EMIT pagxDocumentChanged(pagxDocument);

  // Save XML content for deferred update. The actual documentXmlText assignment happens in
  // onRenderCompleted() after the first render finishes, avoiding races between editor updates
  // and texture presentation.
  pendingXmlContent = xmlString;

  resetView();

  return true;
}

void PAGXViewModel::firstFrame() {
  setIsPlaying(false);
  setProgress(0);
}

void PAGXViewModel::lastFrame() {
  setIsPlaying(false);
  setProgress(1.0);
}

void PAGXViewModel::nextFrame() {
  setIsPlaying(false);
  auto newProgress = progress + progressPerFrame;
  if (newProgress > 1.0) {
    newProgress = 0.0;
  }
  setProgress(newProgress);
}

void PAGXViewModel::previousFrame() {
  setIsPlaying(false);
  auto newProgress = progress - progressPerFrame;
  if (newProgress < 0.0) {
    newProgress = 1.0;
  }
  setProgress(newProgress);
}

void PAGXViewModel::clearContent() {
  pagxDocument = nullptr;
  scene = nullptr;
  defaultAnimation = nullptr;
  defaultLoopMode = pagx::LoopMode::Once;
}

void PAGXViewModel::resolveDefaultAnimation(const std::shared_ptr<pagx::PAGXDocument>& document) {
  defaultAnimation = nullptr;
  defaultLoopMode = pagx::LoopMode::Once;
  if (!document->animations.empty()) {
    auto* firstAnim = document->animations[0];
    if (firstAnim != nullptr && firstAnim->nodeType() == pagx::NodeType::Animation) {
      auto* anim = static_cast<pagx::Animation*>(firstAnim);
      defaultAnimation = scene->getAnimation(anim->id);
      defaultLoopMode = anim->loop;
    }
  }
}

void PAGXViewModel::emitContentStateReset() {
  Q_EMIT progressChanged(0.0);
  Q_EMIT isPlayingChanged(hasAnimation());
  Q_EMIT totalFrameChanged();
  Q_EMIT hasAnimationChanged(hasAnimation());
}

void PAGXViewModel::updateAnimationState() {
  if (defaultAnimation != nullptr && defaultAnimation->duration() > 0) {
    auto durationUs = defaultAnimation->duration();
    auto rate = defaultAnimation->frameRate();
    totalFrames =
        static_cast<int64_t>(std::round(static_cast<double>(durationUs) * rate / 1000000.0));
    if (totalFrames < 1) {
      totalFrames = 1;
    }
    frameRate = rate;
    progressPerFrame = 1.0 / static_cast<double>(totalFrames);
  } else {
    totalFrames = 1;
    frameRate = 0.0f;
    progressPerFrame = 1.0;
  }
  progress = 0.0;
  // Bump the generation on every content rebuild so any progress or finish notification still
  // queued from the previously loaded content is treated as stale and dropped on the main thread,
  // even when the new content does not animate.
  playbackGeneration++;
  isPlaying_ = hasAnimation();
}

QString PAGXViewModel::documentXml() const {
  return documentXmlText;
}

QString PAGXViewModel::applyXmlChanges(const QString& newXml) {
  auto xmlBytes = newXml.toUtf8();
  auto document = pagx::PAGXImporter::FromXML(reinterpret_cast<const uint8_t*>(xmlBytes.data()),
                                              static_cast<size_t>(xmlBytes.length()));
  if (document == nullptr) {
    return tr("Failed to parse XML: invalid syntax or structure");
  }
  document->applyLayout();

  auto newScene = pagx::PAGScene::Make(document);
  if (newScene == nullptr) {
    return tr("Failed to build PAGScene from XML document");
  }

  {
    std::lock_guard<std::mutex> lock(renderMutex);
    pagxDocument = document;
    pagxWidth = static_cast<int>(document->width);
    pagxHeight = static_cast<int>(document->height);
    scene = newScene;
    resolveDefaultAnimation(document);
    updateAnimationState();
    needsRender = true;
  }

  Q_EMIT widthChanged(pagxWidth);
  Q_EMIT heightChanged(pagxHeight);
  emitContentStateReset();
  Q_EMIT contentSizeChanged();
  Q_EMIT pagxDocumentChanged(pagxDocument);
  Q_EMIT requestFlush();

  return {};  // Empty string means success
}

QString PAGXViewModel::saveXmlToFile(const QString& xml) {
  if (currentFilePath.empty()) {
    return tr("No file path specified");
  }
  QFile file(QString::fromLocal8Bit(currentFilePath.data()));
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    return tr("Failed to open file for writing: %1").arg(file.errorString());
  }
  auto xmlBytes = xml.toUtf8();
  qint64 bytesWritten = file.write(xmlBytes);
  if (bytesWritten != xmlBytes.length()) {
    return tr("Failed to write all data to file");
  }
  file.close();

  return {};  // Empty string means success
}

QString PAGXViewModel::validateXml(const QString& xml) const {
  QXmlStreamReader reader(xml);
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.hasError()) {
      return tr("Line %1, column %2: %3")
          .arg(reader.lineNumber())
          .arg(reader.columnNumber())
          .arg(reader.errorString());
    }
  }
  return {};  // Empty string means the XML is well-formed
}

void PAGXViewModel::attachHighlighter(QObject* quickTextDocument) {
  auto* quickDocument = qobject_cast<QQuickTextDocument*>(quickTextDocument);
  if (quickDocument == nullptr) {
    return;
  }
  auto* document = quickDocument->textDocument();
  if (highlighter != nullptr && highlighter->document() == document) {
    return;
  }
  // A previous highlighter can only belong to a previous editor instance's document; replace
  // it so a recreated editor is never left unhighlighted.
  EditorLog("attachHighlighter: attaching to a new document");
  // Plain-text layout: it sizes the document from block count * line height and only lays out
  // a block's content when the block actually enters the viewport. The default rich-text
  // layout instead lays out every block during document-size passes, so megabyte-long base64
  // lines each cost a multi-second synchronous layout while loading (and again whenever the
  // document size is queried after edits).
  document->setDocumentLayout(new QPlainTextDocumentLayout(document));
  delete highlighter;
  highlighter = new XmlDocumentHighlighter(document);
  // Diagnostic probe: every emission means the text backend finished a document-size layout
  // pass, which is where hidden full-document layouts show up.
  connect(document->documentLayout(), &QAbstractTextDocumentLayout::documentSizeChanged, this,
          &PAGXViewModel::onDocumentSizeChanged);
}

void PAGXViewModel::loadEditorText(QObject* quickTextDocument, const QString& text) {
  EditorLog(QString("loadEditorText: text.size=%1").arg(text.size()));
  auto* quickDocument = qobject_cast<QQuickTextDocument*>(quickTextDocument);
  if (quickDocument == nullptr) {
    EditorLog("loadEditorText: invalid quick document, finishing immediately");
    Q_EMIT editorLoadFinished(0);
    return;
  }
  auto* document = quickDocument->textDocument();
  constexpr qsizetype SmallDocumentThreshold = 256 * 1024;
  if (text.size() <= SmallDocumentThreshold) {
    QElapsedTimer timer;
    timer.start();
    document->setPlainText(text);
    const auto setMs = timer.restart();
    document->clearUndoRedoStacks();
    const auto maxLineWidth = MeasureMaxLineWidth(document, text);
    EditorLog(QString("loadEditorText: small path done, setPlainText=%1ms measure=%2ms "
                      "maxLineWidth=%3")
                  .arg(setMs)
                  .arg(timer.elapsed())
                  .arg(maxLineWidth));
    Q_EMIT editorLoadFinished(maxLineWidth);
    return;
  }
  // Replacing the whole text at once would make the attached highlighter rehighlight every
  // block synchronously and freeze the UI for seconds on large files, so large documents are
  // appended in chunks instead: each insert only rehighlights its own range.
  EditorLog("loadEditorText: chunked path starting");
  loaderElapsed.start();
  loaderDocument = document;
  loaderText = text;
  loaderOffset = 0;
  loaderChunkCount = 0;
  loaderMaxLineWidth = 0;
  document->clear();
  if (loaderTimer == nullptr) {
    loaderTimer = new QTimer(this);
    loaderTimer->setInterval(0);
    connect(loaderTimer, &QTimer::timeout, this, &PAGXViewModel::appendEditorChunk);
  }
  loaderTimer->start();
}

void PAGXViewModel::appendEditorChunk() {
  if (loaderDocument == nullptr) {
    loaderTimer->stop();
    loaderText.clear();
    EditorLog("appendEditorChunk: document gone, finishing");
    Q_EMIT editorLoadFinished(loaderMaxLineWidth);
    return;
  }
  constexpr qsizetype ChunkSize = 128 * 1024;
  auto end = qMin(loaderText.size(), loaderOffset + ChunkSize);
  if (end < loaderText.size()) {
    // Cut on a line boundary so every chunk holds complete lines and the highlighter's
    // cross-line states stay consistent.
    const auto newLine = loaderText.lastIndexOf(u'\n', end);
    if (newLine > loaderOffset) {
      end = newLine + 1;
    } else {
      // No line boundary within the window: a single line longer than ChunkSize (e.g. an
      // embedded base64 payload). Extend the chunk to the end of that line so the line is
      // inserted in ONE piece; growing it across chunks would relayout the whole line after
      // every insert, which is quadratic in the line length (multi-second stalls per chunk
      // for megabyte-long lines, as seen in the [PAGXEditor] logs).
      const auto nextNewLine = loaderText.indexOf(u'\n', end);
      end = nextNewLine < 0 ? loaderText.size() : nextNewLine + 1;
      EditorLog(
          QString("appendEditorChunk: huge line detected, chunkBytes=%1").arg(end - loaderOffset));
    }
  }
  QElapsedTimer timer;
  timer.start();
  const auto chunk = QStringView(loaderText).mid(loaderOffset, end - loaderOffset).toString();
  loaderMaxLineWidth = qMax(loaderMaxLineWidth, MeasureMaxLineWidth(loaderDocument, chunk));
  const auto measureMs = timer.restart();
  QTextCursor cursor(loaderDocument);
  cursor.movePosition(QTextCursor::End);
  cursor.beginEditBlock();
  cursor.insertText(chunk);
  cursor.endEditBlock();
  const auto insertMs = timer.elapsed();
  ++loaderChunkCount;
  if (loaderChunkCount % 8 == 0 || end >= loaderText.size()) {
    EditorLog(QString("appendEditorChunk: chunk=%1 bytes=%2/%3 measureMs=%4 insertMs=%5 "
                      "totalMs=%6 maxLineWidth=%7")
                  .arg(loaderChunkCount)
                  .arg(end)
                  .arg(loaderText.size())
                  .arg(measureMs)
                  .arg(insertMs)
                  .arg(loaderElapsed.elapsed())
                  .arg(loaderMaxLineWidth));
  }
  loaderOffset = end;
  if (loaderOffset >= loaderText.size()) {
    loaderTimer->stop();
    loaderText.clear();
    loaderDocument->clearUndoRedoStacks();
    EditorLog(QString("appendEditorChunk: chunked load finished, chunks=%1 totalMs=%2 "
                      "maxLineWidth=%3")
                  .arg(loaderChunkCount)
                  .arg(loaderElapsed.elapsed())
                  .arg(loaderMaxLineWidth));
    Q_EMIT editorLoadFinished(loaderMaxLineWidth);
  }
}

void PAGXViewModel::onDocumentSizeChanged(const QSizeF& size) {
  EditorLog(QString("documentSizeChanged: %1x%2").arg(size.width()).arg(size.height()));
}

void PAGXViewModel::clearDocumentXml() {
  documentXmlText.clear();
  pendingXmlContent.clear();
  Q_EMIT documentXmlChanged();
}

void PAGXViewModel::onRenderCompleted() {
  if (pendingXmlContent.isEmpty()) {
    return;
  }
  documentXmlText = std::move(pendingXmlContent);
  pendingXmlContent.clear();
  Q_EMIT documentXmlChanged();
}

}  // namespace pag
