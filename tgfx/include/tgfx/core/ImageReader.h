/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <atomic>
#include <mutex>
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageBuffer.h"
#include "tgfx/core/ImageStream.h"
#include "tgfx/core/Rect.h"

namespace tgfx {
class Texture;

/**
 * The ImageReader class allows direct access to ImageBuffers generated from an ImageStream. The
 * ImageStream may come from a Bitmap, a Rasterizer, or a video-related object of the native
 * platform. You should call ImageStream::acquireNextBuffer() to read a new ImageBuffer each time
 * when the ImageStream is modified. All ImageBuffers generated from one ImageReader share the same
 * internal texture, which allows you to continuously read the latest content from the ImageStream
 * with minimal memory copying. However, there are two limits:
 *
 *    1) The generated ImageBuffers are bound to the associated GPU Context when first being drawn
 *       and cannot be drawn to another Context anymore.
 *    2) The generated ImageBuffers may have a limited lifetime and cannot create textures after
 *       expiration. Usually, the previously acquired ImageBuffer will expire after the newly
 *       created ImageBuffer is drawn. So there are only two ImageBuffers that can be accessed
 *       simultaneously. But if the ImageStream is backed by a hardware buffer, the previously
 *       acquired ImageBuffer immediately expires when the ImageStream is being modified.
 *
 * You can create multiple ImageReaders from the same ImageStream. ImageStream is safe across
 * threads.
 */
class ImageReader {
 public:
  /**
   * Creates a new ImageReader from the specified Bitmap. Returns nullptr if the bitmap is empty.
   */
  static std::shared_ptr<ImageReader> MakeFrom(const Bitmap& bitmap);

  /**
   * Creates a new ImageReader from the specified ImageStream. Returns nullptr if the imageStream is
   * nullptr.
   */
  static std::shared_ptr<ImageReader> MakeFrom(std::shared_ptr<ImageStream> imageStream);

  virtual ~ImageReader();

  /**
   * Returns the width of generated image buffers.
   */
  int width() const;

  /**
   * Returns the height of generated image buffers.
   */
  int height() const;

  /**
   * Acquires the next ImageBuffer from the ImageReader after a new image frame has been rendered
   * into the associated ImageStream. Usually, the previously acquired ImageBuffer will expire after
   * the newly created ImageBuffer is drawn. But if the ImageStream is backed by a hardware buffer,
   * the previously acquired ImageBuffer immediately expires when the ImageStream is being modified.
   * Returns nullptr if the associated ImageStream has no content changes.
   */
  std::shared_ptr<ImageBuffer> acquireNextBuffer();

 private:
  std::mutex locker = {};
  std::weak_ptr<ImageReader> weakThis;
  std::shared_ptr<ImageStream> stream = nullptr;
  std::shared_ptr<Texture> texture = nullptr;
  uint64_t bufferVersion = 0;
  uint64_t textureVersion = 0;
  Rect dirtyBounds = Rect::MakeEmpty();

  explicit ImageReader(std::shared_ptr<ImageStream> stream);

  bool checkExpired(uint64_t contentVersion);

  std::shared_ptr<Texture> readTexture(uint64_t contentVersion, Context* context, bool mipMapped);

  void onContentDirty(const Rect& bounds);

  friend class ImageReaderBuffer;
  friend class ImageStream;
};
}  // namespace tgfx
