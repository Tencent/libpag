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
#include <memory>
#include <string>

namespace tgfx {
class Data;
}  // namespace tgfx

namespace pagx {

class Image;

bool GetPNGDimensions(const uint8_t* data, size_t size, int* width, int* height);

bool GetPNGDimensionsFromPath(const std::string& path, int* width, int* height);

bool GetImagePNGDimensions(const Image* image, int* width, int* height);

bool GetJPEGDimensions(const uint8_t* data, size_t size, int* width, int* height);

bool GetWebPDimensions(const uint8_t* data, size_t size, int* width, int* height);

bool GetImageDimensions(const Image* image, int* width, int* height);

/**
 * Reads the physical DPI from a raw PNG byte stream. Returns false when the
 * stream is not PNG, the pHYs chunk is absent, or the unit is not meters.
 */
bool GetPNGDPI(const uint8_t* data, size_t size, float* dpiX, float* dpiY);

/**
 * Reads the physical DPI from a raw JPEG byte stream via the JFIF APP0
 * segment. Returns false when the stream is not JPEG or no JFIF density is
 * declared.
 */
bool GetJPEGDPI(const uint8_t* data, size_t size, float* dpiX, float* dpiY);

bool GetImageDPI(const Image* image, float* dpiX, float* dpiY);

bool IsJPEG(const uint8_t* data, size_t size);

bool IsWebP(const uint8_t* data, size_t size);

std::shared_ptr<tgfx::Data> ConvertWebPToPNG(const std::shared_ptr<tgfx::Data>& webpData);

std::shared_ptr<tgfx::Data> GetImageData(const Image* image);

}  // namespace pagx
