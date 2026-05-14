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
#include <string>
#include "pagx/types/Data.h"
#include "pagx/nodes/Node.h"

namespace tgfx {
class Image;
}

namespace pagx {

/**
 * Image represents an image resource that can be referenced by other nodes. The image source can
 * be a file path, a URL, or a base64-encoded data URI.
 */
class Image : public Node {
 public:
  /**
   * Image binary data (decoded from base64).
   */
  std::shared_ptr<Data> data = nullptr;

  /**
   * External file path (mutually exclusive with data, data has priority).
   */
  std::string filePath = {};

  /**
   * A pre-decoded tgfx::Image produced by the host runtime, typically via an async native
   * decoder on platforms where synchronous codec decoding is expensive (e.g. WeChat mini-program
   * where wasm libwebp decoding blocks the main thread). When non-null, the renderer uses this
   * directly and skips encoded-bytes decoding. Populated on the WeChat platform by
   * PAGXView::attachNativeImage() with quality=Full (and the legacy
   * loadFileDataAsNativeImage() / upgradeImageFromNative entry points which forward to it);
   * other platforms currently leave it null.
   */
  std::shared_ptr<tgfx::Image> decodedImage = nullptr;

  /**
   * Low-resolution preview tgfx::Image attached alongside the full-resolution decodedImage.
   * Populated by PAGXView::attachNativeImage() with quality=Thumbnail. When the renderer cannot
   * use decodedImage (initial load not yet complete, or the full-resolution texture has been
   * evicted under memory pressure), it falls back to thumbnailImage so the affected fill area
   * shows a blurry preview rather than a blank rectangle. Always nullptr on platforms that do
   * not opt into the backend-texture image flow.
   */
  std::shared_ptr<tgfx::Image> thumbnailImage = nullptr;

  NodeType nodeType() const override {
    return NodeType::Image;
  }

 private:
  Image() = default;

  friend class PAGXDocument;
};

}  // namespace pagx
