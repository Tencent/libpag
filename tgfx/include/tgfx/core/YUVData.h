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

#include <memory>

namespace tgfx {
/**
 * YUVData represents an array of yuv pixels located in the CPU memory. YUVData holds several planes
 * of immutable buffers. Not only is the YUVData immutable, but the internal buffers are guaranteed
 * to always be the same for the life of this instance.
 */
class YUVData {
 public:
  /**
   * The number of planes required by the I420 format.
   */
  static constexpr size_t I420_PLANE_COUNT = 3;

  /**
   * The number of planes required by the NV12 format.
   */
  static constexpr size_t NV12_PLANE_COUNT = 2;

  /**
   * Function that, if provided, will be called when the YUVData goes out of scope, allowing for
   * custom freeing of the YUVData's contents.
   */
  typedef void (*ReleaseProc)(void* context, const void** data, size_t planeCount);

  /**
   * Creates a YUVData object, taking ownership of the specified data, and using the releaseProc
   * to free it. If the releaseProc is nullptr, the caller must ensure that data are valid for the
   * lifetime of the returned YUVData.
   * @param width The width of the yuv pixels
   * @param height The height of the yuv pixels.
   * @param data The array of base addresses for the planes.
   * @param rowBytes The array of bytes-per-row values for the planes.
   * @param planeCount The number of planes.
   * @param releaseProc The callback function that gets called when the YUVData is destroyed.
   * @param context A pointer to user data identifying the YUVData. This value is passed to your
   * release callback function.
   */
  static std::shared_ptr<YUVData> MakeFrom(int width, int height, const void** data,
                                           const size_t* rowBytes, size_t planeCount,
                                           ReleaseProc releaseProc = nullptr,
                                           void* context = nullptr);

  virtual ~YUVData() = default;

  /**
   * Returns the width of the yuv pixels.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of the yuv pixels.
   */
  virtual int height() const = 0;

  /**
   * Returns number of planes in this yuv data.
   */
  virtual size_t planeCount() const = 0;

  /**
   * Returns the base address of the plane at the specified plane index.
   */
  virtual const void* getBaseAddressAt(int planeIndex) const = 0;

  /**
   * Returns the number of bytes per row for a plane at the specified plane index.
   */
  virtual size_t getRowBytesAt(int planeIndex) const = 0;
};
}  // namespace tgfx
