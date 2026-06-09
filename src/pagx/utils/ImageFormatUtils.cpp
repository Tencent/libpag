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

#include "pagx/utils/ImageFormatUtils.h"
#include <cstring>
#include <fstream>
#include "pagx/nodes/Image.h"
#include "pagx/utils/Base64.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"

namespace pagx {

static const uint8_t PNG_SIGNATURE[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

// Reads a big-endian 16-bit unsigned integer at `data`.
// Used by JPEG (segment length, SOF width/height, JFIF density) decoders.
static uint16_t ReadBE16(const uint8_t* data) {
  return static_cast<uint16_t>((static_cast<uint16_t>(data[0]) << 8) |
                               static_cast<uint16_t>(data[1]));
}

// Reads a big-endian 32-bit unsigned integer at `data`.
// Used by PNG (chunk lengths, IHDR width/height, pHYs ppm) decoders.
static uint32_t ReadBE32(const uint8_t* data) {
  return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16) |
         (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

bool GetPNGDimensions(const uint8_t* data, size_t size, int* width, int* height) {
  if (size < 24) {
    return false;
  }
  if (memcmp(data, PNG_SIGNATURE, 8) != 0) {
    return false;
  }
  *width = static_cast<int>(ReadBE32(data + 16));
  *height = static_cast<int>(ReadBE32(data + 20));
  return *width > 0 && *height > 0;
}

bool GetPNGDimensionsFromPath(const std::string& path, int* width, int* height) {
  if (path.rfind("data:", 0) == 0) {
    auto decoded = DecodeBase64DataURI(path);
    if (!decoded) {
      return false;
    }
    return GetPNGDimensions(decoded->bytes(), decoded->size(), width, height);
  }
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  uint8_t header[24];
  if (!file.read(reinterpret_cast<char*>(header), 24)) {
    return false;
  }
  return GetPNGDimensions(header, 24, width, height);
}

bool GetImagePNGDimensions(const Image* image, int* width, int* height) {
  if (image->data) {
    return GetPNGDimensions(image->data->bytes(), image->data->size(), width, height);
  }
  if (!image->filePath.empty()) {
    return GetPNGDimensionsFromPath(image->filePath, width, height);
  }
  return false;
}

bool GetJPEGDimensions(const uint8_t* data, size_t size, int* width, int* height) {
  if (size < 2 || data[0] != 0xFF || data[1] != 0xD8) {
    return false;
  }
  size_t offset = 2;
  while (offset + 4 < size) {
    if (data[offset] != 0xFF) {
      return false;
    }
    uint8_t marker = data[offset + 1];
    if (marker == 0xD9 || marker == 0xDA) {
      break;
    }
    auto segmentLength = static_cast<size_t>(ReadBE16(data + offset + 2)) + 2;
    // SOF0..SOF3 markers contain image dimensions.
    if (marker >= 0xC0 && marker <= 0xC3 && offset + 9 < size) {
      *height = static_cast<int>(ReadBE16(data + offset + 5));
      *width = static_cast<int>(ReadBE16(data + offset + 7));
      return *width > 0 && *height > 0;
    }
    offset += segmentLength;
  }
  return false;
}

bool GetWebPDimensions(const uint8_t* data, size_t size, int* width, int* height) {
  if (size < 16 || memcmp(data, "RIFF", 4) != 0 || memcmp(data + 8, "WEBP", 4) != 0) {
    return false;
  }
  if (size < 20) {
    return false;
  }
  const uint8_t* chunk = data + 12;
  size_t remaining = size - 12;
  if (remaining >= 18 && memcmp(chunk, "VP8 ", 4) == 0) {
    const uint8_t* vp8 = chunk + 8;
    size_t vp8Size = remaining - 8;
    if (vp8Size >= 10 && vp8[3] == 0x9D && vp8[4] == 0x01 && vp8[5] == 0x2A) {
      *width = static_cast<int>((vp8[6] | (vp8[7] << 8)) & 0x3FFF);
      *height = static_cast<int>((vp8[8] | (vp8[9] << 8)) & 0x3FFF);
      return *width > 0 && *height > 0;
    }
  } else if (remaining >= 13 && memcmp(chunk, "VP8L", 4) == 0) {
    const uint8_t* vp8l = chunk + 8;
    size_t vp8lSize = remaining - 8;
    if (vp8lSize >= 5 && vp8l[0] == 0x2F) {
      uint32_t bits = vp8l[1] | (static_cast<uint32_t>(vp8l[2]) << 8) |
                      (static_cast<uint32_t>(vp8l[3]) << 16) |
                      (static_cast<uint32_t>(vp8l[4]) << 24);
      *width = static_cast<int>((bits & 0x3FFF) + 1);
      *height = static_cast<int>(((bits >> 14) & 0x3FFF) + 1);
      return *width > 0 && *height > 0;
    }
  } else if (remaining >= 18 && memcmp(chunk, "VP8X", 4) == 0) {
    const uint8_t* vp8x = chunk + 8;
    *width = static_cast<int>((vp8x[4] | (vp8x[5] << 8) | (vp8x[6] << 16)) + 1);
    *height = static_cast<int>((vp8x[7] | (vp8x[8] << 8) | (vp8x[9] << 16)) + 1);
    return *width > 0 && *height > 0;
  }
  return false;
}

bool GetImageDimensions(const Image* image, int* width, int* height) {
  if (GetImagePNGDimensions(image, width, height)) {
    return true;
  }
  auto data = GetImageData(image);
  if (data && data->size() > 0) {
    if (GetJPEGDimensions(data->bytes(), data->size(), width, height)) {
      return true;
    }
    return GetWebPDimensions(data->bytes(), data->size(), width, height);
  }
  return false;
}

bool GetPNGDPI(const uint8_t* data, size_t size, float* dpiX, float* dpiY) {
  if (size < 8 || memcmp(data, PNG_SIGNATURE, 8) != 0) {
    return false;
  }
  size_t offset = 8;
  while (offset + 12 <= size) {
    auto chunkLen = ReadBE32(data + offset);
    if (memcmp(data + offset + 4, "pHYs", 4) == 0) {
      if (chunkLen == 9 && offset + 8 + 9 <= size) {
        const uint8_t* d = data + offset + 8;
        auto ppuX = ReadBE32(d);
        auto ppuY = ReadBE32(d + 4);
        uint8_t unit = d[8];
        if (unit == 1 && ppuX > 0 && ppuY > 0) {
          *dpiX = static_cast<float>(ppuX) * 0.0254f;
          *dpiY = static_cast<float>(ppuY) * 0.0254f;
          return true;
        }
      }
      return false;
    }
    if (memcmp(data + offset + 4, "IDAT", 4) == 0) {
      break;
    }
    // Use size_t addition to avoid uint32_t wrap-around that could freeze the
    // loop on a malformed PNG with chunkLen near 0xFFFFFFF4.
    offset += 12 + static_cast<size_t>(chunkLen);
  }
  return false;
}

bool GetJPEGDPI(const uint8_t* data, size_t size, float* dpiX, float* dpiY) {
  if (size < 2 || data[0] != 0xFF || data[1] != 0xD8) {
    return false;
  }
  size_t offset = 2;
  while (offset + 4 < size) {
    if (data[offset] != 0xFF) {
      return false;
    }
    uint8_t marker = data[offset + 1];
    if (marker == 0xD9 || marker == 0xDA) {
      break;
    }
    auto segLen = ReadBE16(data + offset + 2);
    if (marker == 0xE0 && segLen >= 16 && offset + 2 + segLen <= size) {
      if (memcmp(data + offset + 4, "JFIF\0", 5) == 0) {
        uint8_t units = data[offset + 11];
        auto xDensity = ReadBE16(data + offset + 12);
        auto yDensity = ReadBE16(data + offset + 14);
        if (xDensity > 0 && yDensity > 0) {
          if (units == 1) {
            *dpiX = static_cast<float>(xDensity);
            *dpiY = static_cast<float>(yDensity);
            return true;
          } else if (units == 2) {
            *dpiX = static_cast<float>(xDensity) * 2.54f;
            *dpiY = static_cast<float>(yDensity) * 2.54f;
            return true;
          }
        }
      }
    }
    offset += 2 + segLen;
  }
  return false;
}

bool GetImageDPI(const Image* image, float* dpiX, float* dpiY) {
  auto data = GetImageData(image);
  if (!data || data->size() == 0) {
    return false;
  }
  if (GetPNGDPI(data->bytes(), data->size(), dpiX, dpiY)) {
    return true;
  }
  return GetJPEGDPI(data->bytes(), data->size(), dpiX, dpiY);
}

bool IsJPEG(const uint8_t* data, size_t size) {
  return size >= 2 && data[0] == 0xFF && data[1] == 0xD8;
}

bool IsWebP(const uint8_t* data, size_t size) {
  return size >= 12 && memcmp(data, "RIFF", 4) == 0 && memcmp(data + 8, "WEBP", 4) == 0;
}

std::shared_ptr<tgfx::Data> ConvertWebPToPNG(const std::shared_ptr<tgfx::Data>& webpData) {
  if (!webpData) {
    return nullptr;
  }
  auto codec = tgfx::ImageCodec::MakeFrom(webpData);
  if (!codec) {
    return nullptr;
  }
  tgfx::Bitmap bitmap(codec->width(), codec->height(), false, false);
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!codec->readPixels(pixmap.info(), pixmap.writablePixels())) {
    return nullptr;
  }
  return tgfx::ImageCodec::Encode(pixmap, tgfx::EncodedFormat::PNG, 100);
}

std::shared_ptr<tgfx::Data> GetImageData(const Image* image) {
  if (image->data) {
    return tgfx::Data::MakeWithoutCopy(image->data->bytes(), image->data->size());
  }
  if (!image->filePath.empty()) {
    return tgfx::Data::MakeFromFile(image->filePath);
  }
  return nullptr;
}

}  // namespace pagx
