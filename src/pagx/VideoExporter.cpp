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
#include "pagx/VideoExportFrameImpl.h"
#include "pagx/VideoExportInternal.h"
#include "pagx/nodes/Animation.h"
#include "pagx/runtime/Drawable.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"

namespace pagx {

namespace {

constexpr int DefaultFrameRate = 60;

constexpr char MixedModeError[] = "cannot mix nextFrame() with startNextFrame()";
constexpr char CancelledError[] = "the export sequence was cancelled";

void ReportFailure(const VideoExportFailure& failure, std::string* errorMsg,
                   VideoExportError* errorCode) {
  if (errorMsg != nullptr) {
    *errorMsg = failure.message;
  }
  if (errorCode != nullptr) {
    *errorCode = failure.code;
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

bool ValidateDocumentAnimations(const PAGXDocument& document, VideoExportFailure* failure) {
  std::unordered_set<std::string> timelineIds = {};
  for (const auto* node : document.animations) {
    if (node == nullptr ||
        (node->nodeType() != NodeType::Animation && node->nodeType() != NodeType::StateMachine)) {
      continue;
    }
    if (node->id.empty()) {
      failure->set(VideoExportError::InvalidArgument,
                   "top-level animations and state machines must have non-empty ids");
      return false;
    }
    if (!timelineIds.insert(node->id).second) {
      auto typeName = node->nodeType() == NodeType::Animation ? "animation" : "state machine";
      failure->set(VideoExportError::InvalidArgument,
                   "duplicate top-level " + std::string(typeName) + " id '" + node->id + "'");
      return false;
    }
  }
  return true;
}

}  // namespace

bool VideoExporterImpl::validateOptions(const std::shared_ptr<PAGXDocument>& document,
                                        const VideoExportOptions& options,
                                        VideoExportFailure* failure) const {
  if (document == nullptr) {
    failure->set(VideoExportError::InvalidArgument, "the document is null");
    return false;
  }
  const auto& rate = options.frameRate;
  auto automaticFrameRate = rate.isAutomatic();
  if (!automaticFrameRate &&
      !rate.isWithin(VideoExporter::MIN_FRAME_RATE, VideoExporter::MAX_FRAME_RATE)) {
    failure->set(VideoExportError::InvalidArgument,
                 "the frame rate must be auto {0, 1} or within " +
                     std::to_string(VideoExporter::MIN_FRAME_RATE) + "-" +
                     std::to_string(VideoExporter::MAX_FRAME_RATE) + ", got " +
                     std::to_string(rate.frameRateNumerator) + "/" +
                     std::to_string(rate.frameRateDenominator));
    return false;
  }
  if (!std::isfinite(options.duration)) {
    failure->set(VideoExportError::InvalidArgument, "the duration must be finite");
    return false;
  }
  if (!(options.scale > 0.0f) || !std::isfinite(options.scale)) {
    failure->set(VideoExportError::InvalidArgument, "the scale must be finite and positive");
    return false;
  }
  if (!(document->width > 0.0f) || !(document->height > 0.0f) || !std::isfinite(document->width) ||
      !std::isfinite(document->height)) {
    failure->set(VideoExportError::InvalidArgument,
                 "the document dimensions must be finite and positive");
    return false;
  }
  if ((options.outputWidth == 0) != (options.outputHeight == 0) || options.outputWidth < 0 ||
      options.outputHeight < 0) {
    failure->set(VideoExportError::InvalidArgument,
                 "explicit output dimensions must both be positive or both be zero");
    return false;
  }
  if (options.outputWidth > 0 && options.padToEvenSize) {
    failure->set(VideoExportError::InvalidArgument,
                 "padToEvenSize cannot be combined with explicit output dimensions");
    return false;
  }
  return ValidateDocumentAnimations(*document, failure);
}

bool VideoExporterImpl::resolveOutputSize(const std::shared_ptr<PAGXDocument>& document,
                                          const VideoExportOptions& options, int* width,
                                          int* height, VideoExportFailure* failure) const {
  *width = options.outputWidth;
  *height = options.outputHeight;
  const bool hasExplicitSize = *width > 0 && *height > 0;
  if (hasExplicitSize) {
    return true;
  }
  if (!ComputeVideoOutputDimensions(document->width, document->height, options.scale,
                                    options.padToEvenSize, width, height)) {
    failure->set(VideoExportError::InvalidArgument,
                 "the output dimensions must be finite, positive, and fit in an int");
    return false;
  }
  return true;
}

bool VideoExporterImpl::validateHostSurface(const std::shared_ptr<PAGSurface>& hostSurface,
                                            int width, int height,
                                            VideoExportFailure* failure) const {
  if (hostSurface->width() != width || hostSurface->height() != height) {
    failure->set(VideoExportError::InvalidArgument,
                 "the export surface dimensions do not match the expected output of " +
                     std::to_string(width) + "x" + std::to_string(height));
    return false;
  }
  auto drawable = VideoExportSurfaceAccess::drawable(*hostSurface);
  if (drawable == nullptr) {
    failure->set(VideoExportError::DeviceLost, "the export surface has no render target");
    return false;
  }
  auto device = drawable->getDevice();
  if (device == nullptr) {
    failure->set(VideoExportError::DeviceLost, "the export surface has no GPU device");
    return false;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    failure->set(VideoExportError::DeviceLost, "failed to lock the export surface device");
    return false;
  }
  auto tgfxSurface = drawable->getSurface(context);
  auto validContext = tgfxSurface != nullptr && tgfxSurface->getContext() == context;
  device->unlock();
  if (!validContext) {
    failure->set(VideoExportError::DeviceLost, "the export surface has no valid GPU context");
    return false;
  }
  return true;
}

bool VideoExporterImpl::createOffscreenSurface(int width, int height,
                                               std::shared_ptr<PAGSurface>* surface,
                                               VideoExportFailure* failure) const {
  *surface = PAGSurface::MakeOffscreen(width, height);
  if (*surface == nullptr) {
    failure->set(VideoExportError::RenderFailed, "failed to create an offscreen surface of " +
                                                     std::to_string(width) + "x" +
                                                     std::to_string(height));
    return false;
  }
  return true;
}

bool VideoExporterImpl::createFramePixmap(int width, int height,
                                          std::shared_ptr<tgfx::Pixmap>* pixmap,
                                          VideoExportFailure* failure) const {
  auto bitmap = std::make_shared<tgfx::Bitmap>(width, height, false, false);
  if (bitmap->isEmpty()) {
    failure->set(VideoExportError::OutOfMemory, "failed to allocate the frame bitmap");
    return false;
  }
  // The pixmap holds the pixel reference, so the bitmap only has to live until this point.
  *pixmap = std::make_shared<tgfx::Pixmap>(*bitmap);
  return true;
}

void VideoExporterImpl::attachSceneOptions(const std::shared_ptr<PAGScene>& scene,
                                           const VideoExportOptions& options) {
  auto* displayOptions = scene->getDisplayOptions();
  displayOptions->setRenderMode(options.renderMode);
  displayOptions->setZoomScale(options.scale);
  if (options.background.alpha > 0.0f) {
    displayOptions->setBackgroundColor(options.background);
  }
}

bool ComputeVideoOutputDimensions(float documentWidth, float documentHeight, float scale,
                                  bool padToEvenSize, int* width, int* height) {
  if (width == nullptr || height == nullptr) {
    return false;
  }
  int computedWidth = 0;
  int computedHeight = 0;
  if (!ComputeOutputDimension(documentWidth, scale, padToEvenSize, &computedWidth) ||
      !ComputeOutputDimension(documentHeight, scale, padToEvenSize, &computedHeight)) {
    return false;
  }
  *width = computedWidth;
  *height = computedHeight;
  return true;
}

bool VideoExporterImpl::AddAnimation(const std::shared_ptr<PAGAnimation>& animation,
                                     int64_t offsetUs, bool includeInFrameRate, PlaybackPlan* plan,
                                     VideoExportFailure* failure) {
  if (animation == nullptr || animation->animation == nullptr) {
    failure->set(VideoExportError::InvalidArgument, "failed to instantiate an animation");
    return false;
  }
  const auto* definition = animation->animation;
  auto animationFrameRate = definition->frameRate;
  if (!std::isfinite(animationFrameRate) || animationFrameRate < 0.0f) {
    failure->set(VideoExportError::InvalidArgument,
                 "animation '" + definition->id +
                     "' has an invalid frame rate; expected a finite non-negative value");
    return false;
  }
  int64_t periodUs = 0;
  if (animationFrameRate > 0.0f && definition->duration > 0) {
    auto basePeriodUs = static_cast<double>(definition->duration) / animationFrameRate * 1000000.0;
    if (!std::isfinite(basePeriodUs) ||
        basePeriodUs >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
      failure->set(VideoExportError::TooLong,
                   "animation '" + definition->id + "' has an overflowing playback period");
      return false;
    }
    periodUs = static_cast<int64_t>(basePeriodUs);
    if (definition->loop == LoopMode::PingPong) {
      if (periodUs > std::numeric_limits<int64_t>::max() / 2) {
        failure->set(VideoExportError::TooLong,
                     "animation '" + definition->id + "' has an overflowing playback period");
        return false;
      }
      periodUs *= 2;
    }
  }
  auto effectivePeriodUs = static_cast<double>(periodUs);
  if (definition->loop == LoopMode::Once) {
    effectivePeriodUs = std::max(0.0, effectivePeriodUs + static_cast<double>(offsetUs));
  }
  if (!std::isfinite(effectivePeriodUs) ||
      effectivePeriodUs >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    failure->set(VideoExportError::TooLong,
                 "animation '" + definition->id + "' has an overflowing playback period");
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

bool VideoExporterImpl::CollectNestedPlayback(PAGComposition* composition, PlaybackPlan* plan,
                                              VideoExportFailure* failure) {
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
    if (!AddAnimation(animation, animation->evaluationOffsetUs, false, plan, failure)) {
      return false;
    }
  }
  std::vector<PAGComposition*> childCompositions = {};
  PAGComposition::CollectChildCompositions(composition, childCompositions);
  for (auto* child : childCompositions) {
    if (!CollectNestedPlayback(child, plan, failure)) {
      return false;
    }
  }
  return true;
}

bool VideoExporterImpl::ResolvePlaybackPlan(const std::shared_ptr<PAGScene>& scene,
                                            const std::shared_ptr<PAGXDocument>& document,
                                            PlaybackPlan* plan, VideoExportFailure* failure) {
  for (auto* node : document->animations) {
    if (node == nullptr) {
      continue;
    }
    if (node->nodeType() == NodeType::Animation) {
      auto animation = scene->getAnimation(node->id);
      if (!AddAnimation(animation, 0, true, plan, failure)) {
        return false;
      }
      plan->timelines.emplace_back(std::move(animation));
    } else if (node->nodeType() == NodeType::StateMachine) {
      auto stateMachine = scene->getStateMachineTimeline(node->id);
      if (stateMachine == nullptr) {
        failure->set(VideoExportError::RenderFailed,
                     "failed to instantiate state machine '" + node->id + "'");
        return false;
      }
      if (std::find(plan->timelines.begin(), plan->timelines.end(), stateMachine) ==
          plan->timelines.end()) {
        plan->timelines.emplace_back(std::move(stateMachine));
      }
      plan->hasStateMachine = true;
      plan->hasDynamicContent = true;
    }
  }
  return CollectNestedPlayback(scene->rootComposition().get(), plan, failure);
}

bool VideoExporterImpl::ResolveSampling(const VideoExportOptions& options, const PlaybackPlan& plan,
                                        VideoFrameRate* frameRate, int64_t* frameCount,
                                        VideoExportFailure* failure) {
  auto automaticFrameRate = options.frameRate.isAutomatic();
  if (automaticFrameRate && plan.hasMixedFrameRates) {
    failure->set(VideoExportError::MixedFrameRates,
                 "animations have mixed frame rates; set an explicit sampling frame rate");
    return false;
  }
  *frameRate = options.frameRate;
  if (automaticFrameRate && plan.documentFrameRate > 0.0f) {
    if (plan.documentFrameRate > VideoExporter::MAX_FRAME_RATE) {
      failure->set(VideoExportError::InvalidArgument,
                   "the document frame rate must be finite and no greater than " +
                       std::to_string(VideoExporter::MAX_FRAME_RATE));
      return false;
    }
    if (std::floor(plan.documentFrameRate) != plan.documentFrameRate) {
      failure->set(VideoExportError::InvalidArgument,
                   "the document frame rate must be an integer in automatic mode; set "
                   "an explicit sampling frame rate");
      return false;
    }
    *frameRate = {static_cast<int64_t>(plan.documentFrameRate), 1};
  }
  if (automaticFrameRate && frameRate->frameRateNumerator == 0) {
    *frameRate = {DefaultFrameRate, 1};
  }
  auto samplingRate = frameRate->framesPerSecond();

  float durationSeconds = options.duration;
  if (durationSeconds <= 0.0f) {
    if (plan.hasStateMachine) {
      failure->set(VideoExportError::StateMachineRequiresDuration,
                   "documents with state machines require an explicit duration");
      return false;
    }
    if (!plan.hasDynamicContent) {
      durationSeconds = 1.0f / static_cast<float>(samplingRate);
    } else {
      if (plan.longestDurationUs <= 0) {
        failure->set(VideoExportError::NoDuration,
                     "the document's animations have no duration, pass a duration");
        return false;
      }
      durationSeconds = static_cast<float>(static_cast<double>(plan.longestDurationUs) / 1000000.0);
    }
  }
  auto frameCountValue = static_cast<double>(durationSeconds) * samplingRate;
  if (!std::isfinite(frameCountValue) ||
      frameCountValue >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    failure->set(VideoExportError::TooLong, "the requested frame count is too large");
    return false;
  }
  *frameCount = static_cast<int64_t>(std::llround(frameCountValue));
  if (*frameCount <= 0) {
    failure->set(VideoExportError::InvalidArgument, "the requested sequence is empty");
    return false;
  }
  auto lastFrameTimeUs = static_cast<double>(*frameCount - 1) * 1000000.0 / samplingRate;
  if (lastFrameTimeUs >= static_cast<double>(std::numeric_limits<int64_t>::max())) {
    failure->set(VideoExportError::TooLong, "the requested sequence is too long");
    return false;
  }
  return true;
}

std::shared_ptr<VideoExporterImpl> VideoExporterImpl::Make(std::shared_ptr<PAGXDocument> document,
                                                           const VideoExportOptions& options,
                                                           VideoExportFailure* failure) {
  return MakeInternal(std::move(document), nullptr, options, failure);
}

std::shared_ptr<VideoExporterImpl> VideoExporterImpl::MakeWithSurface(
    std::shared_ptr<PAGXDocument> document, std::shared_ptr<PAGSurface> surface,
    const VideoExportOptions& options, VideoExportFailure* failure) {
  if (surface == nullptr) {
    failure->set(VideoExportError::InvalidArgument, "the export surface is null");
    return nullptr;
  }
  return MakeInternal(std::move(document), std::move(surface), options, failure);
}

std::shared_ptr<VideoExporterImpl> VideoExporterImpl::MakeInternal(
    std::shared_ptr<PAGXDocument> document, std::shared_ptr<PAGSurface> surface,
    const VideoExportOptions& options, VideoExportFailure* failure) {
  auto exporter = std::shared_ptr<VideoExporterImpl>(new VideoExporterImpl());
  if (!exporter->validateOptions(document, options, failure)) {
    return nullptr;
  }
  auto scene = PAGScene::Make(document);
  if (scene == nullptr) {
    failure->set(VideoExportError::RenderFailed, "failed to build the animation scene");
    return nullptr;
  }
  PlaybackPlan plan = {};
  if (!ResolvePlaybackPlan(scene, document, &plan, failure)) {
    return nullptr;
  }
  VideoFrameRate frameRate = {};
  int64_t frameCount = 0;
  if (!ResolveSampling(options, plan, &frameRate, &frameCount, failure)) {
    return nullptr;
  }
  int width = 0;
  int height = 0;
  if (!exporter->resolveOutputSize(document, options, &width, &height, failure)) {
    return nullptr;
  }
  if (surface == nullptr) {
    if (!exporter->createOffscreenSurface(width, height, &surface, failure)) {
      return nullptr;
    }
  } else if (!exporter->validateHostSurface(surface, width, height, failure)) {
    return nullptr;
  }
  // A document without dynamic content reuses one readback buffer for every frame and exposes it
  // through pixels(), so that buffer has to exist up front; a dynamic document allocates it on its
  // first synchronous frame instead and never pays for it when only startNextFrame() is used.
  if (!plan.hasDynamicContent &&
      !exporter->createFramePixmap(width, height, &exporter->pixmap, failure)) {
    return nullptr;
  }
  attachSceneOptions(scene, options);
  exporter->state = std::make_shared<VideoExportState>();
  exporter->scene = std::move(scene);
  exporter->surface = std::move(surface);
  exporter->timelines = std::move(plan.timelines);
  exporter->outputWidth = width;
  exporter->outputHeight = height;
  exporter->samplingFrameRate = frameRate;
  exporter->totalFrames = frameCount;
  exporter->reuseStaticFrame = !plan.hasDynamicContent;
  return exporter;
}

PAGRenderMode VideoExporterImpl::renderMode() const {
  auto* options = scene != nullptr ? scene->getDisplayOptions() : nullptr;
  return options != nullptr ? options->getRenderMode() : PAGRenderMode::Partial;
}

int VideoExporterImpl::width() const {
  return outputWidth;
}

int VideoExporterImpl::height() const {
  return outputHeight;
}

VideoFrameRate VideoExporterImpl::frameRate() const {
  return samplingFrameRate;
}

int64_t VideoExporterImpl::frameCount() const {
  return totalFrames;
}

int64_t VideoExporterImpl::currentFrame() const {
  return state != nullptr ? state->currentFrame : 0;
}

std::string VideoExporterImpl::error() const {
  return state != nullptr ? state->error : "";
}

VideoExportError VideoExporterImpl::errorCode() const {
  return state != nullptr ? state->errorCode : VideoExportError::None;
}

bool VideoExporterImpl::drawNextFrame(VideoExportFailure* failure) {
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
    failure->set(VideoExportError::RenderFailed,
                 "failed to draw frame " + std::to_string(frameIndex));
    return false;
  }
  return true;
}

bool VideoExporterImpl::nextFrame(VideoExportFailure* failure) {
  if (state->failed || state->cancelled) {
    return false;
  }
  if (state->mode == VideoExportMode::Async) {
    markMixedModeFailure();
    failure->set(VideoExportError::MixedMode, MixedModeError);
    if (auto pending = pendingFrame.lock()) {
      pending->impl->fail(VideoExportError::MixedMode, state->error);
    }
    return false;
  }
  if (scene == nullptr || state->currentFrame >= totalFrames) {
    return false;
  }
  if (reuseStaticFrame && state->currentFrame > 0) {
    state->currentFrame++;
    state->mode = VideoExportMode::Sync;
    state->errorCode = VideoExportError::None;
    state->error.clear();
    return true;
  }
  if (pixmap == nullptr && !createFramePixmap(outputWidth, outputHeight, &pixmap, failure)) {
    state->errorCode = failure->code;
    state->error = failure->message;
    return false;
  }
  if (!drawNextFrame(failure)) {
    state->errorCode = failure->code;
    state->error = failure->message;
    return false;
  }
  if (!surface->readPixels(pixmap->writablePixels(), pixmap->rowBytes())) {
    state->errorCode = VideoExportError::ReadbackFailed;
    state->error = "failed to read pixels for frame " + std::to_string(state->currentFrame);
    failure->set(state->errorCode, state->error);
    return false;
  }
  state->currentFrame++;
  state->mode = VideoExportMode::Sync;
  state->errorCode = VideoExportError::None;
  state->error.clear();
  return true;
}

VideoExportStartResult VideoExporterImpl::startNextFrame() {
  if (state->failed || state->cancelled) {
    return {VideoExportStartStatus::Failed, nullptr, state->error, state->errorCode};
  }
  if (state->mode == VideoExportMode::Sync) {
    markMixedModeFailure();
    return {VideoExportStartStatus::Failed, nullptr, state->error, state->errorCode};
  }
  if (auto pending = pendingFrame.lock()) {
    if (!pending->impl->hasEnded()) {
      return {VideoExportStartStatus::Busy, pending, "", VideoExportError::None};
    }
    pendingFrame.reset();
  }
  if (state->currentFrame >= totalFrames) {
    return {VideoExportStartStatus::End, nullptr, "", VideoExportError::None};
  }
  auto frameIndex = state->currentFrame;
  std::shared_ptr<VideoExportFrameImpl> impl = nullptr;
  if (reuseStaticFrame && frameIndex > 0) {
    impl = VideoExportFrameImpl::MakeCPU(state, frameIndex, pixmap);
  } else {
    VideoExportFailure failure = {};
    if (drawNextFrame(&failure)) {
      impl = VideoExportFrameImpl::MakeGPU(state, frameIndex, surface,
                                           reuseStaticFrame ? pixmap : nullptr, &failure);
    }
    if (impl == nullptr) {
      state->error = failure.message;
      state->errorCode = failure.code;
      return {VideoExportStartStatus::Failed, nullptr, failure.message, failure.code};
    }
  }
  auto frame = std::shared_ptr<VideoExportFrame>(new VideoExportFrame(std::move(impl)));
  pendingFrame = frame;
  state->mode = VideoExportMode::Async;
  state->errorCode = VideoExportError::None;
  state->error.clear();
  return {VideoExportStartStatus::FrameStarted, std::move(frame), "", VideoExportError::None};
}

void VideoExporterImpl::cancel() {
  if (state->cancelled) {
    return;
  }
  auto wasFailed = state->failed;
  state->cancelled = true;
  if (!wasFailed) {
    state->error = CancelledError;
    state->errorCode = VideoExportError::Cancelled;
    if (auto pending = pendingFrame.lock()) {
      if (!pending->impl->consumed) {
        pending->impl->fail(VideoExportError::Cancelled, state->error);
      }
    }
  }
  pendingFrame.reset();
}

void VideoExporterImpl::markMixedModeFailure() {
  state->failed = true;
  state->errorCode = VideoExportError::MixedMode;
  state->error = MixedModeError;
}

const uint8_t* VideoExporterImpl::pixels() const {
  if (state != nullptr && state->mode == VideoExportMode::Async) {
    return nullptr;
  }
  return pixmap != nullptr ? static_cast<const uint8_t*>(pixmap->pixels()) : nullptr;
}

size_t VideoExporterImpl::rowBytes() const {
  if (state != nullptr && state->mode == VideoExportMode::Async) {
    return 0;
  }
  return pixmap != nullptr ? pixmap->rowBytes() : 0;
}

VideoExporter::VideoExporter(std::shared_ptr<VideoExporterImpl> exporterImpl)
    : impl(std::move(exporterImpl)) {
}

VideoExporter::~VideoExporter() = default;

std::shared_ptr<VideoExporter> VideoExporter::Make(std::shared_ptr<PAGXDocument> document,
                                                   const Options& options, std::string* errorMsg,
                                                   VideoExportError* errorCode) {
  VideoExportFailure failure = {};
  auto exporter = VideoExporterImpl::Make(std::move(document), options, &failure);
  if (exporter == nullptr) {
    ReportFailure(failure, errorMsg, errorCode);
    return nullptr;
  }
  return std::shared_ptr<VideoExporter>(new VideoExporter(std::move(exporter)));
}

std::shared_ptr<VideoExporter> VideoExporter::MakeWithSurface(
    std::shared_ptr<PAGXDocument> document, std::shared_ptr<PAGSurface> surface,
    const Options& options, std::string* errorMsg, VideoExportError* errorCode) {
  VideoExportFailure failure = {};
  auto exporter = VideoExporterImpl::MakeWithSurface(std::move(document), std::move(surface),
                                                     options, &failure);
  if (exporter == nullptr) {
    ReportFailure(failure, errorMsg, errorCode);
    return nullptr;
  }
  return std::shared_ptr<VideoExporter>(new VideoExporter(std::move(exporter)));
}

PAGRenderMode VideoExporter::renderMode() const {
  return impl->renderMode();
}

int VideoExporter::width() const {
  return impl->width();
}

int VideoExporter::height() const {
  return impl->height();
}

VideoFrameRate VideoExporter::frameRate() const {
  return impl->frameRate();
}

int64_t VideoExporter::frameCount() const {
  return impl->frameCount();
}

int64_t VideoExporter::currentFrame() const {
  return impl->currentFrame();
}

bool VideoExporter::nextFrame(std::string* errorMsg) {
  if (errorMsg != nullptr) {
    errorMsg->clear();
  }
  VideoExportFailure failure = {};
  if (impl->nextFrame(&failure)) {
    return true;
  }
  ReportFailure(failure, errorMsg, nullptr);
  return false;
}

VideoExportStartResult VideoExporter::startNextFrame() {
  return impl->startNextFrame();
}

void VideoExporter::cancel() {
  impl->cancel();
}

std::string VideoExporter::error() const {
  return impl->error();
}

VideoExportError VideoExporter::errorCode() const {
  return impl->errorCode();
}

const uint8_t* VideoExporter::pixels() const {
  return impl->pixels();
}

size_t VideoExporter::rowBytes() const {
  return impl->rowBytes();
}

}  // namespace pagx
