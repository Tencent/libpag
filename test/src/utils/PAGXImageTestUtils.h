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
#include "pagx/PAGXDocument.h"
#include "pagx/nodes/Image.h"
#include "pagx/types/Data.h"

namespace pag {

// Creates an Image node whose bytes are a minimal valid 2x2 RGBA PNG (8-bit,
// non-interlaced). Shared by PAGX exporter tests that need a decodable image
// payload without depending on runtime encoders or external resource files.
inline pagx::Image* MakeTestPNGImage(pagx::PAGXDocument* doc) {
  static const uint8_t kMinimalPNG[] = {
      0x89,
      0x50,
      0x4E,
      0x47,
      0x0D,
      0x0A,
      0x1A,
      0x0A,  // PNG signature
      // IHDR
      0x00,
      0x00,
      0x00,
      0x0D,
      0x49,
      0x48,
      0x44,
      0x52,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      0x00,
      0x00,
      0x02,
      0x08,
      0x02,
      0x00,
      0x00,
      0x00,
      0xFD,
      0xD4,
      0x9A,
      0x73,
      // IDAT (compressed pixel data)
      0x00,
      0x00,
      0x00,
      0x14,
      0x49,
      0x44,
      0x41,
      0x54,
      0x78,
      0x9C,
      0x62,
      0xF8,
      0xCF,
      0xC0,
      0xF0,
      0x1F,
      0x01,
      0x18,
      0x18,
      0x18,
      0x00,
      0x09,
      0x04,
      0x01,
      0x01,
      0xE2,
      0x2D,
      0x42,
      0xA3,
      // IEND
      0x00,
      0x00,
      0x00,
      0x00,
      0x49,
      0x45,
      0x4E,
      0x44,
      0xAE,
      0x42,
      0x60,
      0x82,
  };
  auto* image = doc->makeNode<pagx::Image>();
  image->data = pagx::Data::MakeWithCopy(kMinimalPNG, sizeof(kMinimalPNG));
  return image;
}

}  // namespace pag
