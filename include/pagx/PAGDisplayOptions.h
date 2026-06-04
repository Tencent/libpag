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

#include <memory>
#include "pagx/types/Color.h"
#include "pagx/types/Point.h"

namespace pagx {

class PAGScene;

/**
 * PAGRenderMode defines how a PAGScene display list is rendered to a PAGSurface.
 */
enum class PAGRenderMode {
  /**
   * Renders all layers directly to the target surface without display-list caching.
   */
  Direct,

  /**
   * Renders only dirty regions using a cached previous frame when possible. This is the default
   * mode and can improve performance when only part of the display list changes.
   */
  Partial,

  /**
   * Splits the display list into cached tiles for efficient scrolling and zooming.
   */
  Tiled,
};

/**
 * PAGDisplayOptions exposes display-list rendering options owned by a PAGScene. Use
 * PAGScene::getDisplayOptions() to obtain the options and PAGScene::draw() to render with them.
 */
class PAGDisplayOptions {
 public:
  ~PAGDisplayOptions();

  PAGDisplayOptions(const PAGDisplayOptions&) = delete;
  PAGDisplayOptions& operator=(const PAGDisplayOptions&) = delete;

  /**
   * Returns the scale factor applied to the runtime layer tree during drawing.
   */
  float getZoomScale() const;

  /**
   * Sets the scale factor applied to the runtime layer tree during drawing. Adjusting this value is
   * more cache-friendly than modifying layer transforms for viewport zooming.
   * @param zoomScale the scale factor to apply.
   */
  void setZoomScale(float zoomScale);

  /**
   * Returns the integer precision used when converting zoomScale into cache keys.
   */
  int getZoomScalePrecision() const;

  /**
   * Sets the integer precision used when converting zoomScale into cache keys.
   * @param precision the precision multiplier.
   */
  void setZoomScalePrecision(int precision);

  /**
   * Returns the content offset applied after zoomScale during drawing.
   */
  Point getContentOffset() const;

  /**
   * Sets the content offset applied after zoomScale during drawing. The renderer may round the
   * offset to pixel-aligned values for stable cached rendering.
   * @param offsetX horizontal offset in pixels.
   * @param offsetY vertical offset in pixels.
   */
  void setContentOffset(float offsetX, float offsetY);

  /**
   * Returns the current display-list render mode.
   */
  PAGRenderMode getRenderMode() const;

  /**
   * Sets the display-list render mode.
   * @param renderMode the render mode to use for subsequent draws.
   */
  void setRenderMode(PAGRenderMode renderMode);

  /**
   * Returns the tile size used by tiled rendering mode.
   */
  int getTileSize() const;

  /**
   * Sets the tile size used by tiled rendering mode. This value is ignored by other render modes.
   * @param tileSize tile width and height in pixels.
   */
  void setTileSize(int tileSize);

  /**
   * Returns the maximum number of tiles that can be created in tiled rendering mode.
   */
  int getMaxTileCount() const;

  /**
   * Sets the maximum number of tiles that can be created in tiled rendering mode.
   * @param count maximum tile count; implementation may raise too-small values internally.
   */
  void setMaxTileCount(int count);

  /**
   * Returns whether temporary blurred fallback tiles are allowed while zooming.
   */
  bool getAllowZoomBlur() const;

  /**
   * Sets whether temporary blurred fallback tiles are allowed while zooming in tiled mode.
   * @param allow true to allow fallback tiles from other zoom levels.
   */
  void setAllowZoomBlur(bool allow);

  /**
   * Returns the maximum number of zoom-blur fallback tiles refined per frame.
   */
  int getMaxTilesRefinedPerFrame() const;

  /**
   * Sets the maximum number of zoom-blur fallback tiles refined per frame.
   * @param count maximum number of tiles to refine each frame.
   */
  void setMaxTilesRefinedPerFrame(int count);

  /**
   * Returns the background color drawn behind the runtime layer tree.
   */
  Color getBackgroundColor() const;

  /**
   * Sets the background color drawn behind the runtime layer tree.
   * @param color background color in PAGX color representation.
   */
  void setBackgroundColor(const Color& color);

  /**
   * Returns the maximum longest edge in pixels for static subtree texture caching.
   */
  int getSubtreeCacheMaxSize() const;

  /**
   * Sets the maximum longest edge in pixels for static subtree texture caching. Use 0 to disable
   * subtree caching.
   * @param maxSize maximum cache size in pixels.
   */
  void setSubtreeCacheMaxSize(int maxSize);

 private:
  explicit PAGDisplayOptions(std::shared_ptr<PAGScene> scene);

  void* getDisplayList() const;

  std::weak_ptr<PAGScene> scene = {};

  friend class PAGScene;
};

}  // namespace pagx
