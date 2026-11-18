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
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "pagx/VideoExporter.h"

namespace tgfx {
class Pixmap;
}  // namespace tgfx

namespace pagx {

class Drawable;
class PAGAnimation;
class PAGComposition;
class PAGScene;
class PAGSurface;
class PAGTimeline;
class PAGXDocument;
class VideoExportFrame;
class VideoExportFrameImpl;

/** Carries the message and the stable code of one failure through the exporter implementation. */
struct VideoExportFailure {
  VideoExportError code = VideoExportError::None;
  std::string message = {};

  bool failed() const {
    return code != VideoExportError::None;
  }

  void set(VideoExportError errorCode, std::string errorMessage) {
    code = errorCode;
    message = std::move(errorMessage);
  }
};

/** Which entry point started the sequence; the first successful use fixes it. */
enum class VideoExportMode {
  None,
  Sync,
  Async,
};

/** Shared by the exporter and its current frame without creating an ownership cycle. */
struct VideoExportState {
  VideoExportMode mode = VideoExportMode::None;
  int64_t currentFrame = 0;
  bool failed = false;
  bool cancelled = false;
  VideoExportError errorCode = VideoExportError::None;
  std::string error = {};
};

/** Returns the render target adapter of a surface without exposing it through the public API. */
std::shared_ptr<Drawable> GetSurfaceDrawable(const PAGSurface& surface);

/** Computes scaled and optionally even-padded video dimensions without exposing a public API. */
bool ComputeVideoOutputDimensions(float documentWidth, float documentHeight, float scale,
                                  bool padToEvenSize, int* width, int* height);

/**
 * Implementation behind VideoExporter. It owns the scene, the surface and the playback plan, while
 * the public class only forwards and converts VideoExportFailure into the caller's two out
 * pointers.
 */
class VideoExporterImpl {
 public:
  static std::shared_ptr<VideoExporterImpl> Make(std::shared_ptr<PAGXDocument> document,
                                                 const VideoExportOptions& options,
                                                 VideoExportFailure* failure);
  static std::shared_ptr<VideoExporterImpl> MakeWithSurface(std::shared_ptr<PAGXDocument> document,
                                                            std::shared_ptr<PAGSurface> surface,
                                                            const VideoExportOptions& options,
                                                            VideoExportFailure* failure);

  ~VideoExporterImpl() = default;

  VideoExporterImpl(const VideoExporterImpl&) = delete;
  VideoExporterImpl& operator=(const VideoExporterImpl&) = delete;

  PAGRenderMode renderMode() const;
  int width() const;
  int height() const;
  VideoFrameRate frameRate() const;
  int64_t frameCount() const;
  int64_t currentFrame() const;
  bool nextFrame(VideoExportFailure* failure);
  VideoExportStartResult startNextFrame();
  void cancel();
  std::string error() const;
  VideoExportError errorCode() const;
  const uint8_t* pixels() const;
  size_t rowBytes() const;

 private:
  struct PlaybackPlan {
    std::vector<std::shared_ptr<PAGTimeline>> timelines = {};
    int64_t longestDurationUs = 0;
    float documentFrameRate = 0.0f;
    bool hasMixedFrameRates = false;
    bool hasDynamicContent = false;
    bool hasStateMachine = false;
  };

  VideoExporterImpl() = default;

  static std::shared_ptr<VideoExporterImpl> MakeInternal(std::shared_ptr<PAGXDocument> document,
                                                         std::shared_ptr<PAGSurface> surface,
                                                         const VideoExportOptions& options,
                                                         VideoExportFailure* failure);
  bool validateOptions(const std::shared_ptr<PAGXDocument>& document,
                       const VideoExportOptions& options, VideoExportFailure* failure) const;
  bool resolveOutputSize(const std::shared_ptr<PAGXDocument>& document,
                         const VideoExportOptions& options, int* width, int* height,
                         VideoExportFailure* failure) const;
  bool validateHostSurface(const std::shared_ptr<PAGSurface>& surface, int width, int height,
                           VideoExportFailure* failure) const;
  bool createOffscreenSurface(int width, int height, std::shared_ptr<PAGSurface>* surface,
                              VideoExportFailure* failure) const;
  bool createFramePixmap(int width, int height, std::shared_ptr<tgfx::Pixmap>* pixmap,
                         VideoExportFailure* failure) const;
  static void attachSceneOptions(const std::shared_ptr<PAGScene>& scene,
                                 const VideoExportOptions& options);
  void markMixedModeFailure();
  bool drawNextFrame(VideoExportFailure* failure);

  static bool ResolvePlaybackPlan(const std::shared_ptr<PAGScene>& scene,
                                  const std::shared_ptr<PAGXDocument>& document, PlaybackPlan* plan,
                                  VideoExportFailure* failure);
  static bool AddAnimation(const std::shared_ptr<PAGAnimation>& animation, int64_t offsetUs,
                           bool includeInFrameRate, PlaybackPlan* plan,
                           VideoExportFailure* failure);
  static bool CollectNestedPlayback(PAGComposition* composition, PlaybackPlan* plan,
                                    VideoExportFailure* failure);
  static bool ResolveSampling(const VideoExportOptions& options, const PlaybackPlan& plan,
                              VideoFrameRate* frameRate, int64_t* frameCount,
                              VideoExportFailure* failure);

  std::shared_ptr<VideoExportState> state = {};
  std::shared_ptr<VideoExportFrame> pendingFrame = {};
  std::shared_ptr<PAGScene> scene = {};
  std::shared_ptr<PAGSurface> surface = {};
  std::vector<std::shared_ptr<PAGTimeline>> timelines = {};
  std::shared_ptr<tgfx::Pixmap> pixmap = {};
  int outputWidth = 0;
  int outputHeight = 0;
  VideoFrameRate samplingFrameRate = {60, 1};
  int64_t totalFrames = 0;
  int64_t previousFrameTimeUs = 0;
  bool reuseStaticFrame = false;
};

}  // namespace pagx
