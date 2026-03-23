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

#pragma once

#include <mutex>
#include "audio/PAGAudioPlayer.h"
#include "pag/pag.h"
#include "rendering/ContentViewModel.h"

class QQuickWindow;

namespace pag {

/**
 * ViewModel for PAG format content. Owns PAGPlayer, PAGFile and PAGAudioPlayer,
 * and manages playback state for PAG animations.
 */
class PAGViewModel : public ContentViewModel {
  Q_OBJECT
 public:
  explicit PAGViewModel(QObject* parent = nullptr);

  int getWidth() const override;
  int getHeight() const override;
  bool hasAnimation() const override;
  bool isPlaying() const override;
  double getProgress() const override;
  QString getTotalFrame() const override;
  QString getCurrentFrame() const override;
  QString getDuration() const override;
  QString getFilePath() const override;
  QString getDisplayedTime() const override;
  QColor getBackgroundColor() const override;
  QSizeF getPreferredSize() const override;
  int getEditableTextLayerCount() const override;
  int getEditableImageLayerCount() const override;
  bool getShowVideoFrames() const override;
  ContentType getContentType() const override;

  void setIsPlaying(bool isPlaying) override;
  void setProgress(double progress) override;
  void setShowVideoFrames(bool isShow) override;

  Q_INVOKABLE bool loadFile(const QString& filePath) override;
  Q_INVOKABLE void firstFrame() override;
  Q_INVOKABLE void lastFrame() override;
  Q_INVOKABLE void nextFrame() override;
  Q_INVOKABLE void previousFrame() override;

  bool hasAudio() const;
  void setWindow(QQuickWindow* window);
  PAGPlayer* getPAGPlayer() const;
  PAGFile* getPAGFile() const;
  void setProgressInternal(double progress, bool isAudioSeek);

  Q_SIGNAL void fileChanged(std::shared_ptr<pag::File> file);
  Q_SIGNAL void pagFileChanged(std::shared_ptr<pag::PAGFile> pagFile);
  Q_SIGNAL void audioTimeChanged(int64_t audioTime);

 private:
  QQuickWindow* window = nullptr;
  mutable std::mutex progressMutex = {};
  int editableTextLayerCount = 0;
  int editableImageLayerCount = 0;
  bool isPlaying_ = true;
  double progress = 0.0;
  double progressPerFrame = 0.0;
  std::unique_ptr<PAGPlayer> pagPlayer = nullptr;
  std::shared_ptr<PAGFile> pagFile = nullptr;
  std::unique_ptr<PAGAudioPlayer> audioPlayer = nullptr;
};

}  // namespace pag
