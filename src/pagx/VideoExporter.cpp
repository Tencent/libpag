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

#include "pagx/VideoExporter.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>
#include "pagx/PAGAnimation.h"
#include "pagx/PAGDisplayOptions.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/VideoExportFrame.h"
#include "pagx/nodes/Animation.h"
#include "pagx/runtime/Drawable.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"

namespace pagx {

namespace {

constexpr int kMaxFrameRate = 120;

constexpr int kDefaultFrameRate = 60;

void SetError(std::string* errorMsg, const std::string& text) {
  if (errorMsg != nullptr) {
    *errorMsg = text;
  }
}

bool ComputeOutputDimension(float documentSize, float scale, bool alignToEvenSize, int* dimension) {
  auto scaledSize = std::ceil(static_cast<double>(documentSize) * scale);
  if (!std::isfinite(scaledSize) || scaledSize <= 0.0 ||
      scaledSize > static_cast<double>(std::numeric_limits<int>::max() - 1)) {
    return false;
  }
  auto value = static_cast<int>(scaledSize);
  if (alignToEvenSize) {
    value += value % 2;
  }
  *dimension = value;
  return true;
}

bool HasTimelineDefinitions(const PAGXDocument& document) {
  return std::any_of(document.nodes.begin(), document.nodes.end(), [](const auto& node) {
    return node != nullptr &&
           (node->nodeType() == NodeType::Animation || node->nodeType() == NodeType::StateMachine);
  });
}

bool ValidateDocumentAnimations(const PAGXDocument& document, std::string* errorMsg) {
  std::unordered_set<std::string> animationIds = {};
  for (const auto* node : document.animations) {
    if (node == nullptr || node->nodeType() != NodeType::Animation) {
      continue;
    }
    const auto* animation = static_cast<const Animation*>(node);
    if (animation->id.empty()) {
      SetError(errorMsg, "top-level animations must have non-empty ids");
      return false;
    }
    if (!animationIds.insert(animation->id).second) {
      SetError(errorMsg, "duplicate top-level animation id '" + animation->id + "'");
      return false;
    }
    auto frameRate = animation->frameRate;
    if (frameRate == 0.0f) {
      continue;
    }
    if (!std::isfinite(frameRate) || frameRate < 0.0f) {
      SetError(errorMsg, "animation '" + animation->id +
                             "' has an invalid frame rate; expected a finite non-negative value");
      return false;
    }
    if (animation->duration <= 0) {
      continue;
    }
    auto periodUs = static_cast<double>(animation->duration) / frameRate * 1000000.0;
    if (animation->loop == LoopMode::PingPong) {
      periodUs *= 2.0;
    }
    if (!std::isfinite(periodUs) ||
        periodUs >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
      SetError(errorMsg, "animation '" + animation->id + "' has an overflowing playback period");
      return false;
    }
  }
  return true;
}

}  // namespace

VideoExporter::VideoExporter() = default;

VideoExporter::~VideoExporter() = default;

std::shared_ptr<VideoExporter> VideoExporter::Make(std::shared_ptr<PAGXDocument> document,
                                                   const Options& options, std::string* errorMsg) {
  return MakeInternal(std::move(document), nullptr, options, errorMsg);
}

std::shared_ptr<VideoExporter> VideoExporter::MakeWithSurface(
    std::shared_ptr<PAGXDocument> document, std::shared_ptr<PAGSurface> surface,
    const Options& options, std::string* errorMsg) {
  if (surface == nullptr) {
    SetError(errorMsg, "the export surface is null");
    return nullptr;
  }
  return MakeInternal(std::move(document), std::move(surface), options, errorMsg);
}

std::shared_ptr<VideoExporter> VideoExporter::MakeInternal(std::shared_ptr<PAGXDocument> document,
                                                           std::shared_ptr<PAGSurface> surface,
                                                           const Options& options,
                                                           std::string* errorMsg) {
  if (document == nullptr) {
    SetError(errorMsg, "the document is null");
    return nullptr;
  }
  if (options.frameRate < 0 || options.frameRate > kMaxFrameRate) {
    SetError(errorMsg, "the frame rate must be within 0-" + std::to_string(kMaxFrameRate) +
                           ", got " + std::to_string(options.frameRate));
    return nullptr;
  }
  if (!std::isfinite(options.duration)) {
    SetError(errorMsg, "the duration must be finite");
    return nullptr;
  }
  if (!(options.scale > 0.0f) || !std::isfinite(options.scale)) {
    SetError(errorMsg, "the scale must be finite and positive");
    return nullptr;
  }
  if (!(document->width > 0.0f) || !(document->height > 0.0f) || !std::isfinite(document->width) ||
      !std::isfinite(document->height)) {
    SetError(errorMsg, "the document dimensions must be finite and positive");
    return nullptr;
  }
  if (!ValidateDocumentAnimations(*document, errorMsg)) {
    return nullptr;
  }
  auto scene = PAGScene::Make(document);
  if (scene == nullptr) {
    SetError(errorMsg, "failed to build the animation scene");
    return nullptr;
  }

  std::vector<std::shared_ptr<PAGAnimation>> animations = {};
  int64_t longestDurationUs = 0;
  float documentFrameRate = 0.0f;
  bool hasMixedFrameRates = false;
  for (const auto& id : scene->getAnimationIds()) {
    auto animation = scene->getAnimation(id);
    if (animation == nullptr) {
      continue;
    }
    auto animationFrameRate = animation->frameRate();
    if (animationFrameRate > 0.0f) {
      if (documentFrameRate <= 0.0f) {
        documentFrameRate = animationFrameRate;
      } else if (documentFrameRate != animationFrameRate) {
        hasMixedFrameRates = true;
      }
    }
    longestDurationUs = std::max(longestDurationUs, animation->playbackPeriod());
    animations.emplace_back(std::move(animation));
  }
  if (options.frameRate == 0 && hasMixedFrameRates) {
    SetError(errorMsg,
             "top-level animations have mixed frame rates; set an explicit sampling "
             "frame rate");
    return nullptr;
  }

  int frameRate = options.frameRate;
  if (frameRate == 0 && documentFrameRate > 0.0f) {
    if (!std::isfinite(documentFrameRate) || documentFrameRate > kMaxFrameRate) {
      SetError(errorMsg, "the document frame rate must be finite and no greater than " +
                             std::to_string(kMaxFrameRate));
      return nullptr;
    }
    if (std::floor(documentFrameRate) != documentFrameRate) {
      SetError(errorMsg,
               "the document frame rate must be an integer when frameRate is zero; set "
               "an explicit sampling frame rate");
      return nullptr;
    }
    frameRate = static_cast<int>(documentFrameRate);
  }
  if (frameRate <= 0) {
    frameRate = kDefaultFrameRate;
  }

  float durationSeconds = options.duration;
  if (durationSeconds <= 0.0f) {
    if (animations.empty()) {
      durationSeconds = 1.0f / static_cast<float>(frameRate);
    } else {
      if (longestDurationUs <= 0) {
        SetError(errorMsg, "the document's animations have no duration, pass a duration");
        return nullptr;
      }
      durationSeconds = static_cast<float>(static_cast<double>(longestDurationUs) / 1000000.0);
    }
  }
  auto frameCountValue = static_cast<double>(durationSeconds) * frameRate;
  if (!std::isfinite(frameCountValue) ||
      frameCountValue >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    SetError(errorMsg, "the requested frame count is too large");
    return nullptr;
  }
  auto frameCount = static_cast<int64_t>(std::llround(frameCountValue));
  if (frameCount <= 0) {
    SetError(errorMsg, "the requested sequence is empty");
    return nullptr;
  }
  auto lastFrameTimeUs = static_cast<double>(frameCount - 1) * 1000000.0 / frameRate;
  if (lastFrameTimeUs >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    SetError(errorMsg, "the requested sequence is too long");
    return nullptr;
  }

  float scale = options.scale;
  int width = options.outputWidth;
  int height = options.outputHeight;
  const bool hasExplicitSize = width > 0 && height > 0;
  if ((width == 0) != (height == 0) || width < 0 || height < 0) {
    SetError(errorMsg, "explicit output dimensions must both be positive or both be zero");
    return nullptr;
  }
  if (!hasExplicitSize &&
      (!ComputeOutputDimension(document->width, scale, options.alignToEvenSize, &width) ||
       !ComputeOutputDimension(document->height, scale, options.alignToEvenSize, &height))) {
    SetError(errorMsg, "the output dimensions must be finite, positive, and fit in an int");
    return nullptr;
  }

  if (surface == nullptr) {
    surface = PAGSurface::MakeOffscreen(width, height);
    if (surface == nullptr) {
      SetError(errorMsg, "failed to create an offscreen surface of " + std::to_string(width) + "x" +
                             std::to_string(height));
      return nullptr;
    }
  } else {
    if (surface->width() != width || surface->height() != height) {
      SetError(errorMsg, "the export surface dimensions do not match the expected output of " +
                             std::to_string(width) + "x" + std::to_string(height));
      return nullptr;
    }
    auto device = surface->drawable->getDevice();
    if (device == nullptr) {
      SetError(errorMsg, "the export surface has no GPU device");
      return nullptr;
    }
    auto* context = device->lockContext();
    if (context == nullptr) {
      SetError(errorMsg, "failed to lock the export surface device");
      return nullptr;
    }
    auto tgfxSurface = surface->drawable->getSurface(context);
    auto validContext = tgfxSurface != nullptr && tgfxSurface->getContext() == context;
    device->unlock();
    if (!validContext) {
      SetError(errorMsg, "the export surface has no valid GPU context");
      return nullptr;
    }
  }
  auto bitmap = std::make_shared<tgfx::Bitmap>(width, height, false, false);
  if (bitmap->isEmpty()) {
    SetError(errorMsg, "failed to allocate the frame bitmap");
    return nullptr;
  }

  auto* displayOptions = scene->getDisplayOptions();
  displayOptions->setRenderMode(options.renderMode);
  displayOptions->setZoomScale(scale);
  if (options.hasBackground) {
    displayOptions->setBackgroundColor(options.background);
  }

  auto exporter = std::shared_ptr<VideoExporter>(new VideoExporter());
  exporter->state = std::make_shared<VideoExportState>();
  exporter->scene = std::move(scene);
  exporter->surface = std::move(surface);
  exporter->animations = std::move(animations);
  exporter->bitmap = std::move(bitmap);
  exporter->pixmap = std::make_shared<tgfx::Pixmap>(*exporter->bitmap);
  exporter->width_ = width;
  exporter->height_ = height;
  exporter->frameRate_ = frameRate;
  exporter->frameCount_ = frameCount;
  exporter->reuseStaticFrame_ = !HasTimelineDefinitions(*document);
  return exporter;
}

PAGRenderMode VideoExporter::renderMode() const {
  auto* options = scene != nullptr ? scene->getDisplayOptions() : nullptr;
  return options != nullptr ? options->getRenderMode() : PAGRenderMode::Partial;
}

int VideoExporter::width() const {
  return width_;
}

int VideoExporter::height() const {
  return height_;
}

int VideoExporter::frameRate() const {
  return frameRate_;
}

int64_t VideoExporter::frameCount() const {
  return frameCount_;
}

int64_t VideoExporter::currentFrame() const {
  return state != nullptr ? state->currentFrame : 0;
}

std::string VideoExporter::error() const {
  return state != nullptr ? state->error : "";
}

bool VideoExporter::drawNextFrame(std::string* errorMsg) {
  auto frameIndex = state->currentFrame;
  auto timeUs = static_cast<int64_t>(
      std::llround(static_cast<double>(frameIndex) * 1000000.0 / static_cast<double>(frameRate_)));
  for (const auto& animation : animations) {
    animation->setCurrentTime(timeUs);
    animation->apply(1.0f);
  }
  // Timelines mounted on Composition slots only advance incrementally, so the scene must be driven
  // by the delta between consecutive frames even though the top-level animations above are seeked
  // to an absolute time.
  scene->advanceAndApply(timeUs - previousTimeUs_);
  previousTimeUs_ = timeUs;
  if (!scene->draw(surface)) {
    SetError(errorMsg, "failed to draw the export frame");
    return false;
  }
  return true;
}

bool VideoExporter::nextFrame() {
  if (state->failed || state->cancelled) {
    return false;
  }
  if (state->mode == VideoExportMode::Async) {
    state->failed = true;
    state->error = "mixed_mode";
    if (pendingFrame != nullptr) {
      pendingFrame->impl->fail(state->error);
    }
    return false;
  }
  if (scene == nullptr || state->currentFrame >= frameCount_) {
    return false;
  }
  if (reuseStaticFrame_ && state->currentFrame > 0) {
    state->currentFrame++;
    state->mode = VideoExportMode::Sync;
    state->error.clear();
    return true;
  }
  std::string renderError = {};
  if (!drawNextFrame(&renderError)) {
    state->error = renderError;
    return false;
  }
  if (!surface->readPixels(pixmap->writablePixels(), pixmap->rowBytes())) {
    state->error = "failed to read the export frame pixels";
    return false;
  }
  state->currentFrame++;
  state->mode = VideoExportMode::Sync;
  state->error.clear();
  return true;
}

VideoExportStartResult VideoExporter::startNextFrame() {
  if (state->failed || state->cancelled) {
    return {VideoExportStartStatus::Failed, nullptr, state->error};
  }
  if (state->mode == VideoExportMode::Sync) {
    state->failed = true;
    state->error = "mixed_mode";
    return {VideoExportStartStatus::Failed, nullptr, state->error};
  }
  if (pendingFrame != nullptr) {
    if (pendingFrame->impl->consumed) {
      pendingFrame = nullptr;
    } else {
      return {VideoExportStartStatus::Busy, nullptr, ""};
    }
  }
  if (state->currentFrame >= frameCount_) {
    return {VideoExportStartStatus::End, nullptr, ""};
  }
  auto frameIndex = state->currentFrame;
  std::shared_ptr<VideoExportFrameImpl> impl = nullptr;
  if (reuseStaticFrame_ && frameIndex > 0) {
    impl = VideoExportFrameImpl::MakeCPU(state, frameIndex, pixmap);
  } else {
    std::string renderError = {};
    if (!drawNextFrame(&renderError)) {
      state->failed = true;
      state->error = renderError;
      return {VideoExportStartStatus::Failed, nullptr, renderError};
    }
    impl = VideoExportFrameImpl::MakeGPU(state, frameIndex, surface,
                                         reuseStaticFrame_ ? pixmap : nullptr, &renderError);
    if (impl == nullptr) {
      state->failed = true;
      state->error = renderError;
      return {VideoExportStartStatus::Failed, nullptr, renderError};
    }
  }
  auto frame = std::shared_ptr<VideoExportFrame>(new VideoExportFrame(std::move(impl)));
  pendingFrame = frame;
  state->mode = VideoExportMode::Async;
  state->error.clear();
  return {VideoExportStartStatus::FrameStarted, std::move(frame), ""};
}

void VideoExporter::cancel() {
  if (state->cancelled) {
    return;
  }
  auto wasFailed = state->failed;
  state->cancelled = true;
  if (!wasFailed) {
    state->error = "cancelled";
    if (pendingFrame != nullptr && !pendingFrame->impl->consumed) {
      pendingFrame->impl->fail(state->error);
    }
  }
  pendingFrame = nullptr;
}

const uint8_t* VideoExporter::pixels() const {
  if (pixmap == nullptr) {
    return nullptr;
  }
  return static_cast<const uint8_t*>(pixmap->writablePixels());
}

size_t VideoExporter::rowBytes() const {
  if (pixmap == nullptr) {
    return 0;
  }
  return pixmap->rowBytes();
}

}  // namespace pagx
