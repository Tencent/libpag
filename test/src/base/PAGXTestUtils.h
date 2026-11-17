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
//  license is distributed on an "AS IS" BASIS, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Pixmap.h"

namespace pag {

// The PAGX suites author their documents at this size, so one offscreen surface covers every
// rendered frame they compare.
inline constexpr int PAGX_TEST_DOC_SIZE = 200;

// Tolerance for the pose / geometry assertions. Interpolated poses and the runtime's float math
// drift well below this bound, while every authored value in the suites is an exact multiple.
inline constexpr float PAGX_TEST_EPSILON = 0.001f;

/**
 * Renders one frame of `doc` through the runtime path (`PAGScene` + `PAGSurface`), which is how the
 * CLI and the player draw, and returns tightly packed RGBA pixels. `timeUs` seeks the document
 * animations before drawing; 0 keeps them at their initial state. Returns an empty vector when the
 * document or the surface cannot be created, or when the pixels cannot be read back.
 */
inline std::vector<uint8_t> RenderFrame(const std::shared_ptr<pagx::PAGXDocument>& doc,
                                        int64_t timeUs = 0) {
  std::vector<uint8_t> pixels = {};
  if (doc == nullptr) {
    return pixels;
  }
  auto scene = pagx::PAGScene::Make(doc);
  if (scene == nullptr) {
    return pixels;
  }
  if (timeUs != 0) {
    for (const auto& id : scene->getAnimationIds()) {
      auto animation = scene->getAnimation(id);
      if (animation != nullptr) {
        animation->setCurrentTime(timeUs);
        animation->apply(1.0f);
      }
    }
  }
  scene->advanceAndApply(timeUs);
  auto surface = pagx::PAGSurface::MakeOffscreen(PAGX_TEST_DOC_SIZE, PAGX_TEST_DOC_SIZE);
  if (surface == nullptr || !scene->draw(surface)) {
    return {};
  }
  tgfx::Bitmap bitmap(PAGX_TEST_DOC_SIZE, PAGX_TEST_DOC_SIZE, false, false);
  if (bitmap.isEmpty()) {
    return {};
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.writablePixels(), pixmap.rowBytes())) {
    return {};
  }
  const auto* src = reinterpret_cast<const uint8_t*>(pixmap.writablePixels());
  pixels.assign(src, src + static_cast<size_t>(pixmap.rowBytes()) * PAGX_TEST_DOC_SIZE);
  return pixels;
}

/**
 * Counts the pixels whose RGBA values differ between `a` and `b`. Returns `a.size() + 1` when the
 * two frames have different sizes or are empty, so a failed render never compares equal to another
 * failed render.
 */
inline size_t CountDifferentPixels(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
  if (a.size() != b.size() || a.empty()) {
    return a.size() + 1;
  }
  size_t different = 0;
  for (size_t i = 0; i + 3 < a.size(); i += 4) {
    if (std::memcmp(&a[i], &b[i], 4) != 0) {
      ++different;
    }
  }
  return different;
}

/**
 * Returns true when `a` and `b` are within `eps` of each other.
 */
inline bool NearlyEqual(float a, float b, float eps = PAGX_TEST_EPSILON) {
  return std::fabs(a - b) <= eps;
}

/**
 * Returns true when `doc` reports at least one diagnostic that contains `needle`. The importer
 * merges every diagnostic into one list, so the needle is the only way to assert on the reason.
 */
inline bool HasDiagnosticContaining(const std::shared_ptr<pagx::PAGXDocument>& doc,
                                    const std::string& needle) {
  if (doc == nullptr) {
    return false;
  }
  for (const auto& message : doc->errors) {
    if (message.find(needle) != std::string::npos) {
      return true;
    }
  }
  return false;
}

}  // namespace pag
