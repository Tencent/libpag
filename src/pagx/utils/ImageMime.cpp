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

#include "pagx/utils/ImageMime.h"

namespace pagx {

const char* DetectImageMime(const uint8_t* bytes, size_t size) {
  if (bytes == nullptr) {
    return nullptr;
  }
  // PNG: 89 50 4E 47 0D 0A 1A 0A
  if (size >= 8 && bytes[0] == 0x89 && bytes[1] == 'P' && bytes[2] == 'N' && bytes[3] == 'G') {
    return "image/png";
  }
  // JPEG: FF D8 FF
  if (size >= 3 && bytes[0] == 0xFF && bytes[1] == 0xD8 && bytes[2] == 0xFF) {
    return "image/jpeg";
  }
  // WebP: "RIFF....WEBP". We accept the RIFF prefix here for parity with the existing
  // HTML writer detector; downstream decoders (tgfx) sniff the full "WEBP" tag.
  if (size >= 4 && bytes[0] == 'R' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == 'F') {
    return "image/webp";
  }
  // GIF: "GIF87a" or "GIF89a"
  if (size >= 4 && bytes[0] == 'G' && bytes[1] == 'I' && bytes[2] == 'F' && bytes[3] == '8') {
    return "image/gif";
  }
  return nullptr;
}

const char* DetectImageMimeOrPNG(const uint8_t* bytes, size_t size) {
  const char* mime = DetectImageMime(bytes, size);
  return mime ? mime : "image/png";
}

}  // namespace pagx
