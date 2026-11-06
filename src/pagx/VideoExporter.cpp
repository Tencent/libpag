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
#include <vector>
#include "pagx/PAGAnimation.h"
#include "pagx/PAGDisplayOptions.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"

namespace pagx {

namespace {

void SetError(std::string* errorMsg, const std::string& text) {
  if (errorMsg != nullptr) {
    *errorMsg = text;
  }
}

}  // namespace

VideoExporter::VideoExporter() = default;

VideoExporter::~VideoExporter() = default;

std::shared_ptr<VideoExporter> VideoExporter::Make(std::shared_ptr<PAGXDocument> document,
                                                   const Options& options, std::string* errorMsg) {
  if (document == nullptr) {
    SetError(errorMsg, "the document is null");
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
  for (const auto& id : scene->getAnimationIds()) {
    auto animation = scene->getAnimation(id);
    if (animation == nullptr) {
      continue;
    }
    if (documentFrameRate <= 0.0f) {
      documentFrameRate = animation->frameRate();
    }
    longestDurationUs = std::max(longestDurationUs, animation->duration());
    animations.emplace_back(std::move(animation));
  }
  if (animations.empty()) {
    SetError(errorMsg, "the document has no top-level animation to export");
    return nullptr;
  }

  int frameRate = options.frameRate;
  if (frameRate <= 0) {
    frameRate = static_cast<int>(std::llround(static_cast<double>(documentFrameRate)));
  }
  if (frameRate <= 0) {
    frameRate = 60;
  }

  float durationSeconds = options.duration;
  if (durationSeconds <= 0.0f) {
    if (longestDurationUs <= 0) {
      SetError(errorMsg, "the document's animations have no duration, pass a duration");
      return nullptr;
    }
    durationSeconds = static_cast<float>(static_cast<double>(longestDurationUs) / 1000000.0);
  }
  auto frameCount = static_cast<int64_t>(
      std::llround(static_cast<double>(durationSeconds) * static_cast<double>(frameRate)));
  if (frameCount <= 0) {
    SetError(errorMsg, "the requested sequence is empty");
    return nullptr;
  }

  float scale = options.scale;
  if (!(scale > 0.0f)) {
    scale = 1.0f;
  }
  int width = static_cast<int>(std::ceil(static_cast<double>(document->width) * scale));
  int height = static_cast<int>(std::ceil(static_cast<double>(document->height) * scale));
  if (options.alignToEvenSize) {
    width += width % 2;
    height += height % 2;
  }
  if (width <= 0 || height <= 0) {
    SetError(errorMsg, "the output dimensions are zero");
    return nullptr;
  }

  auto surface = PAGSurface::MakeOffscreen(width, height);
  if (surface == nullptr) {
    SetError(errorMsg, "failed to create an offscreen surface of " + std::to_string(width) + "x" +
                           std::to_string(height));
    return nullptr;
  }
  auto bitmap = std::make_shared<tgfx::Bitmap>(width, height, false, false);
  if (bitmap->isEmpty()) {
    SetError(errorMsg, "failed to allocate the frame bitmap");
    return nullptr;
  }

  auto* displayOptions = scene->getDisplayOptions();
  displayOptions->setZoomScale(scale);
  if (options.hasBackground) {
    displayOptions->setBackgroundColor(options.background);
  }

  auto exporter = std::shared_ptr<VideoExporter>(new VideoExporter());
  exporter->scene = std::move(scene);
  exporter->surface = std::move(surface);
  exporter->animations = std::move(animations);
  exporter->bitmap = std::move(bitmap);
  exporter->pixmap = std::make_shared<tgfx::Pixmap>(*exporter->bitmap);
  exporter->width_ = width;
  exporter->height_ = height;
  exporter->frameRate_ = frameRate;
  exporter->frameCount_ = frameCount;
  return exporter;
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
  return currentFrame_;
}

bool VideoExporter::nextFrame() {
  if (scene == nullptr || currentFrame_ >= frameCount_) {
    return false;
  }
  auto timeUs = static_cast<int64_t>(std::llround(static_cast<double>(currentFrame_) * 1000000.0 /
                                                  static_cast<double>(frameRate_)));
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
    return false;
  }
  if (!surface->readPixels(pixmap->writablePixels(), pixmap->rowBytes())) {
    return false;
  }
  currentFrame_++;
  return true;
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
