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

#include <cstdint>

namespace pag {

class GPUDrawable;

/**
 * Interface for format-specific rendering logic used by RenderThread.
 * Implementations are responsible for executing one frame of rendering and reporting metrics.
 */
class IContentRenderer {
 public:
  struct RenderMetrics {
    int64_t renderTime = 0;
    int64_t presentTime = 0;
    int64_t imageDecodeTime = 0;
    // Frame index within the animation; -1 means not applicable (e.g. static PAGX content).
    int64_t currentFrame = -1;
    // True if rendering actually occurred; false if the frame was skipped.
    bool rendered = false;
  };

  virtual ~IContentRenderer() = default;

  /**
   * Performs one frame of rendering. Called from the render thread.
   * Returns metrics for the completed frame, with rendered=true if rendering occurred, or
   * rendered=false if the frame was skipped.
   */
  virtual RenderMetrics flush() = 0;

  /**
   * Notifies the renderer that the drawable surface size has changed. Called from the render thread.
   */
  virtual void updateSize() = 0;

  /**
   * Returns true if the renderer has all required resources ready to render.
   */
  virtual bool isReady() const = 0;

  /**
   * Sets the drawable surface for rendering. Default implementation does nothing.
   * Override in renderers that need direct drawable access.
   */
  virtual void setDrawable(GPUDrawable* drawable) {
    (void)drawable;
  }
};

}  // namespace pag
