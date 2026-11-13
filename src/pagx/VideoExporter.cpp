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
#include "pagx/PAGComposition.h"
#include "pagx/PAGDisplayOptions.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGStateMachine.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGTimeline.h"
#include "pagx/PAGXDocument.h"
#include "pagx/VideoExportFrame.h"
#include "pagx/nodes/Animation.h"
#include "pagx/runtime/Drawable.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"

namespace pagx {

namespace {

constexpr int DefaultFrameRate = 60;

void SetError(std::string* errorMsg, const std::string& text) {
  if (errorMsg != nullptr) {
    *errorMsg = text;
  }
}

bool ComputeOutputDimension(float documentSize, float scale, bool padToEvenSize, int* dimension) {
  auto scaledSize = std::ceil(static_cast<double>(documentSize) * scale);
  if (!std::isfinite(scaledSize) || scaledSize <= 0.0 ||
      scaledSize > static_cast<double>(std::numeric_limits<int>::max() - 1)) {
    return false;
  }
  auto value = static_cast<int>(scaledSize);
  if (padToEvenSize) {
    value += value % 2;
  }
  *dimension = value;
  return true;
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
  }
  return true;
}

bool ValidateOptions(const std::shared_ptr<PAGXDocument>& document,
                     const VideoExportOptions& options, std::string* errorMsg) {
  if (document == nullptr) {
    SetError(errorMsg, "the document is null");
    return false;
  }
  const auto& rate = options.frameRate;
  auto automaticFrameRate = rate.numerator == 0 && rate.denominator == 1;
  if (!automaticFrameRate &&
      !rate.isWithin(VideoExporter::MIN_FRAME_RATE, VideoExporter::MAX_FRAME_RATE)) {
    SetError(errorMsg, "the frame rate must be auto {0, 1} or within " +
                           std::to_string(VideoExporter::MIN_FRAME_RATE) + "-" +
                           std::to_string(VideoExporter::MAX_FRAME_RATE) + ", got " +
                           rate.toString());
    return false;
  }
  if (!std::isfinite(options.duration)) {
    SetError(errorMsg, "the duration must be finite");
    return false;
  }
  if (!(options.scale > 0.0f) || !std::isfinite(options.scale)) {
    SetError(errorMsg, "the scale must be finite and positive");
    return false;
  }
  if (!(document->width > 0.0f) || !(document->height > 0.0f) || !std::isfinite(document->width) ||
      !std::isfinite(document->height)) {
    SetError(errorMsg, "the document dimensions must be finite and positive");
    return false;
  }
  return ValidateDocumentAnimations(*document, errorMsg);
}

bool CreateResources(int width, int height, std::shared_ptr<PAGSurface>* surface,
                     std::shared_ptr<tgfx::Bitmap>* bitmap, std::string* errorMsg) {
  *surface = PAGSurface::MakeOffscreen(width, height);
  if (*surface == nullptr) {
    SetError(errorMsg, "failed to create an offscreen surface of " + std::to_string(width) + "x" +
                           std::to_string(height));
    return false;
  }
  *bitmap = std::make_shared<tgfx::Bitmap>(width, height, false, false);
  if ((*bitmap)->isEmpty()) {
    SetError(errorMsg, "failed to allocate the frame bitmap");
    return false;
  }
  return true;
}

}  // namespace

VideoExporter::VideoExporter() = default;

VideoExporter::~VideoExporter() = default;

bool VideoExporter::ComputeOutputDimensions(float documentWidth, float documentHeight, float scale,
                                            bool padToEvenSize, int* width, int* height) {
  if (width == nullptr || height == nullptr) {
    return false;
  }
  return ComputeOutputDimension(documentWidth, scale, padToEvenSize, width) &&
         ComputeOutputDimension(documentHeight, scale, padToEvenSize, height);
}

bool VideoExporter::AddAnimation(const std::shared_ptr<PAGAnimation>& animation, int64_t offsetUs,
                                 bool includeInFrameRate, PlaybackPlan* plan,
                                 std::string* errorMsg) {
  if (animation == nullptr || animation->animation == nullptr) {
    SetError(errorMsg, "failed to instantiate an animation");
    return false;
  }
  const auto* definition = animation->animation;
  auto animationFrameRate = definition->frameRate;
  if (!std::isfinite(animationFrameRate) || animationFrameRate < 0.0f) {
    SetError(errorMsg, "animation '" + definition->id +
                           "' has an invalid frame rate; expected a finite non-negative value");
    return false;
  }
  int64_t periodUs = 0;
  if (animationFrameRate > 0.0f && definition->duration > 0) {
    auto basePeriodUs = static_cast<double>(definition->duration) / animationFrameRate * 1000000.0;
    if (!std::isfinite(basePeriodUs) ||
        basePeriodUs >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
      SetError(errorMsg, "animation '" + definition->id + "' has an overflowing playback period");
      return false;
    }
    periodUs = static_cast<int64_t>(basePeriodUs);
    if (definition->loop == LoopMode::PingPong) {
      if (periodUs > std::numeric_limits<int64_t>::max() / 2) {
        SetError(errorMsg, "animation '" + definition->id + "' has an overflowing playback period");
        return false;
      }
      periodUs *= 2;
    }
  }
  auto effectivePeriodUs = static_cast<double>(periodUs) + static_cast<double>(offsetUs);
  if (!std::isfinite(effectivePeriodUs) ||
      effectivePeriodUs >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    SetError(errorMsg, "animation '" + definition->id + "' has an overflowing playback period");
    return false;
  }
  if (effectivePeriodUs > 0.0) {
    plan->longestDurationUs =
        std::max(plan->longestDurationUs, static_cast<int64_t>(effectivePeriodUs));
  }
  if (includeInFrameRate && animationFrameRate > 0.0f) {
    if (plan->documentFrameRate <= 0.0f) {
      plan->documentFrameRate = animationFrameRate;
    } else if (plan->documentFrameRate != animationFrameRate) {
      plan->hasMixedFrameRates = true;
    }
  }
  plan->hasDynamicContent = true;
  return true;
}

bool VideoExporter::CollectNestedPlayback(PAGComposition* composition, PlaybackPlan* plan,
                                          std::string* errorMsg) {
  if (composition == nullptr) {
    return true;
  }
  for (const auto& timeline : composition->timelines) {
    if (timeline == nullptr) {
      continue;
    }
    if (timeline->type() == TimelineType::StateMachine) {
      plan->hasStateMachine = true;
      plan->hasDynamicContent = true;
      continue;
    }
    if (!composition->isTimelinePlaying(timeline->getId())) {
      continue;
    }
    auto animation = std::static_pointer_cast<PAGAnimation>(timeline);
    if (!AddAnimation(animation, animation->evaluationOffsetUs, false, plan, errorMsg)) {
      return false;
    }
  }
  std::vector<PAGComposition*> childCompositions = {};
  PAGComposition::CollectChildCompositions(composition, childCompositions);
  for (auto* child : childCompositions) {
    if (!CollectNestedPlayback(child, plan, errorMsg)) {
      return false;
    }
  }
  return true;
}

bool VideoExporter::ResolvePlaybackPlan(const std::shared_ptr<PAGScene>& scene,
                                        const std::shared_ptr<PAGXDocument>& document,
                                        PlaybackPlan* plan, std::string* errorMsg) {
  for (auto* node : document->animations) {
    if (node == nullptr) {
      continue;
    }
    if (node->nodeType() == NodeType::Animation) {
      auto animation = scene->getAnimation(node->id);
      if (!AddAnimation(animation, 0, true, plan, errorMsg)) {
        return false;
      }
      plan->timelines.emplace_back(std::move(animation));
    } else if (node->nodeType() == NodeType::StateMachine) {
      auto stateMachine = scene->getStateMachineTimeline(node->id);
      if (stateMachine == nullptr) {
        SetError(errorMsg, "failed to instantiate state machine '" + node->id + "'");
        return false;
      }
      plan->timelines.emplace_back(std::move(stateMachine));
      plan->hasStateMachine = true;
      plan->hasDynamicContent = true;
    }
  }
  return CollectNestedPlayback(scene->rootComposition().get(), plan, errorMsg);
}

bool VideoExporter::ResolveSampling(const Options& options, const PlaybackPlan& plan,
                                    VideoFrameRate* frameRate, int64_t* frameCount,
                                    std::string* errorMsg) {
  auto automaticFrameRate = options.frameRate.numerator == 0;
  if (automaticFrameRate && plan.hasMixedFrameRates) {
    SetError(errorMsg, "animations have mixed frame rates; set an explicit sampling frame rate");
    return false;
  }
  *frameRate = options.frameRate;
  if (automaticFrameRate && plan.documentFrameRate > 0.0f) {
    if (plan.documentFrameRate > MAX_FRAME_RATE) {
      SetError(errorMsg, "the document frame rate must be finite and no greater than " +
                             std::to_string(MAX_FRAME_RATE));
      return false;
    }
    if (std::floor(plan.documentFrameRate) != plan.documentFrameRate) {
      SetError(errorMsg,
               "the document frame rate must be an integer in automatic mode; set "
               "an explicit sampling frame rate");
      return false;
    }
    *frameRate = {static_cast<int64_t>(plan.documentFrameRate), 1};
  }
  if (automaticFrameRate && frameRate->numerator == 0) {
    *frameRate = {DefaultFrameRate, 1};
  }
  auto samplingRate = frameRate->framesPerSecond();

  float durationSeconds = options.duration;
  if (durationSeconds <= 0.0f) {
    if (plan.hasStateMachine) {
      SetError(errorMsg, "documents with state machines require an explicit duration");
      return false;
    }
    if (!plan.hasDynamicContent) {
      durationSeconds = 1.0f / static_cast<float>(samplingRate);
    } else {
      if (plan.longestDurationUs <= 0) {
        SetError(errorMsg, "the document's animations have no duration, pass a duration");
        return false;
      }
      durationSeconds = static_cast<float>(static_cast<double>(plan.longestDurationUs) / 1000000.0);
    }
  }
  auto frameCountValue = static_cast<double>(durationSeconds) * samplingRate;
  if (!std::isfinite(frameCountValue) ||
      frameCountValue >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    SetError(errorMsg, "the requested frame count is too large");
    return false;
  }
  *frameCount = static_cast<int64_t>(std::llround(frameCountValue));
  if (*frameCount <= 0) {
    SetError(errorMsg, "the requested sequence is empty");
    return false;
  }
  auto lastFrameTimeUs = static_cast<double>(*frameCount - 1) * 1000000.0 / samplingRate;
  if (lastFrameTimeUs >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    SetError(errorMsg, "the requested sequence is too long");
    return false;
  }
  return true;
}

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
  if (!ValidateOptions(document, options, errorMsg)) {
    return nullptr;
  }
  auto scene = PAGScene::Make(document);
  if (scene == nullptr) {
    SetError(errorMsg, "failed to build the animation scene");
    return nullptr;
  }
  PlaybackPlan plan = {};
  if (!ResolvePlaybackPlan(scene, document, &plan, errorMsg)) {
    return nullptr;
  }
  VideoFrameRate frameRate = {};
  int64_t frameCount = 0;
  if (!ResolveSampling(options, plan, &frameRate, &frameCount, errorMsg)) {
    return nullptr;
  }
  int width = options.outputWidth;
  int height = options.outputHeight;
  const bool hasExplicitSize = width > 0 && height > 0;
  if ((width == 0) != (height == 0) || width < 0 || height < 0) {
    SetError(errorMsg, "explicit output dimensions must both be positive or both be zero");
    return nullptr;
  }
  if (!hasExplicitSize && !ComputeOutputDimensions(document->width, document->height, options.scale,
                                                   options.padToEvenSize, &width, &height)) {
    SetError(errorMsg, "the output dimensions must be finite, positive, and fit in an int");
    return nullptr;
  }
  std::shared_ptr<tgfx::Bitmap> bitmap = {};
  if (surface == nullptr) {
    if (!CreateResources(width, height, &surface, &bitmap, errorMsg)) {
      return nullptr;
    }
  } else {
    if (surface->width() != width || surface->height() != height) {
      SetError(errorMsg, "the export surface dimensions do not match the expected output of " +
                             std::to_string(width) + "x" + std::to_string(height));
      return nullptr;
    }
    if (surface->drawable == nullptr) {
      SetError(errorMsg, "the export surface has no drawable");
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
    bitmap = std::make_shared<tgfx::Bitmap>(width, height, false, false);
    if (bitmap->isEmpty()) {
      SetError(errorMsg, "failed to allocate the frame bitmap");
      return nullptr;
    }
  }

  auto* displayOptions = scene->getDisplayOptions();
  displayOptions->setRenderMode(options.renderMode);
  displayOptions->setZoomScale(options.scale);
  if (options.background.alpha > 0.0f) {
    displayOptions->setBackgroundColor(options.background);
  }

  auto exporter = std::shared_ptr<VideoExporter>(new VideoExporter());
  exporter->state = std::make_shared<VideoExportState>();
  exporter->scene = std::move(scene);
  exporter->surface = std::move(surface);
  exporter->timelines = std::move(plan.timelines);
  exporter->bitmap = std::move(bitmap);
  exporter->pixmap = std::make_shared<tgfx::Pixmap>(*exporter->bitmap);
  exporter->outputWidth = width;
  exporter->outputHeight = height;
  exporter->samplingFrameRate = frameRate;
  exporter->totalFrames = frameCount;
  exporter->reuseStaticFrame = !plan.hasDynamicContent;
  return exporter;
}

PAGRenderMode VideoExporter::renderMode() const {
  auto* options = scene != nullptr ? scene->getDisplayOptions() : nullptr;
  return options != nullptr ? options->getRenderMode() : PAGRenderMode::Partial;
}

int VideoExporter::width() const {
  return outputWidth;
}

int VideoExporter::height() const {
  return outputHeight;
}

VideoFrameRate VideoExporter::frameRate() const {
  return samplingFrameRate;
}

int64_t VideoExporter::frameCount() const {
  return totalFrames;
}

int64_t VideoExporter::currentFrame() const {
  return state != nullptr ? state->currentFrame : 0;
}

std::string VideoExporter::error() const {
  return state != nullptr ? state->error : "";
}

bool VideoExporter::drawNextFrame(std::string* errorMsg) {
  auto frameIndex = state->currentFrame;
  auto timeUs = static_cast<int64_t>(std::llround(static_cast<double>(frameIndex) * 1000000.0 /
                                                  samplingFrameRate.framesPerSecond()));
  auto deltaUs = timeUs - previousFrameTimeUs;
  for (const auto& timeline : timelines) {
    if (timeline->type() == TimelineType::Animation) {
      auto animation = std::static_pointer_cast<PAGAnimation>(timeline);
      animation->setCurrentTime(timeUs);
      animation->apply(1.0f);
    } else {
      timeline->advanceAndApply(deltaUs);
    }
  }
  // Timelines mounted on Composition slots only advance incrementally, so the scene must be driven
  // by the delta between consecutive frames even though the top-level animations above are seeked
  // to an absolute time.
  scene->advanceAndApply(deltaUs);
  previousFrameTimeUs = timeUs;
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
  if (scene == nullptr || state->currentFrame >= totalFrames) {
    return false;
  }
  if (reuseStaticFrame && state->currentFrame > 0) {
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
  if (state->currentFrame >= totalFrames) {
    return {VideoExportStartStatus::End, nullptr, ""};
  }
  auto frameIndex = state->currentFrame;
  std::shared_ptr<VideoExportFrameImpl> impl = nullptr;
  if (reuseStaticFrame && frameIndex > 0) {
    impl = VideoExportFrameImpl::MakeCPU(state, frameIndex, pixmap);
  } else {
    std::string renderError = {};
    if (!drawNextFrame(&renderError)) {
      state->failed = true;
      state->error = renderError;
      return {VideoExportStartStatus::Failed, nullptr, renderError};
    }
    impl = VideoExportFrameImpl::MakeGPU(state, frameIndex, surface,
                                         reuseStaticFrame ? pixmap : nullptr, &renderError);
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
  return static_cast<const uint8_t*>(pixmap->pixels());
}

size_t VideoExporter::rowBytes() const {
  return pixmap->rowBytes();
}

}  // namespace pagx
