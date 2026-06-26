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

namespace tgfx {
class Image;
}

namespace pagx {

/**
 * Provides pre-decoded image resources to the rendering pipeline. Platform-specific implementations
 * can manage their own caching, eviction, and progressive loading strategies without coupling
 * decoded image state to the document model layer.
 *
 * The renderer queries this provider during layer building. When resolveImage() returns non-null,
 * the renderer uses that image directly and skips all codec-based decoding paths (embedded data,
 * data URI, file path). When it returns nullptr, the renderer falls back to the standard decoding
 * chain.
 *
 * Thread safety: the provider is accessed only on the render thread (inside draw()/flush()), so
 * implementations do not need internal synchronization unless they share state with other threads.
 */
class ImageResourceProvider {
 public:
  virtual ~ImageResourceProvider() = default;

  /**
   * Resolves a pre-decoded image for the given external file path. The implementation decides
   * which quality level to return (full, thumbnail, or nullptr for fallback).
   * @param filePath the external file path as declared in the PAGX document's Image node.
   * @return A tgfx::Image ready for rendering, or nullptr to signal the renderer should decode
   *         from embedded data / file path.
   */
  virtual std::shared_ptr<tgfx::Image> resolveImage(const std::string& filePath) = 0;

  /**
   * Returns true if the provider currently holds any decoded image (full or thumbnail) for the
   * given path. Used by getExternalFilePaths() to exclude paths that already have a decoded
   * counterpart, preventing redundant network fetches.
   * @param filePath the external file path to check.
   */
  virtual bool hasImage(const std::string& filePath) const = 0;
};

}  // namespace pagx
