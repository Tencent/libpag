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
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/utils/ImageMime.h"

namespace pagx {

namespace {

bool MatchesTag(const uint8_t* bytes, const char* tag) {
  return bytes[0] == tag[0] && bytes[1] == tag[1] && bytes[2] == tag[2] && bytes[3] == tag[3];
}

// True when `brand` is the major brand of the ISO-BMFF `ftyp` box at byte 4 or appears in its
// compatible-brand list. AVIF and HEIC files commonly declare the generic `mif1` major brand and
// name their real codec only in the compatible list, so the list has to be consulted to tell the
// two apart. The scan is bounded by the box size so a truncated buffer cannot be walked past.
bool HasIsoBmffBrand(const uint8_t* bytes, size_t size, const char* brand) {
  if (size < 12 || !MatchesTag(bytes + 4, "ftyp")) {
    return false;
  }
  if (MatchesTag(bytes + 8, brand)) {
    return true;
  }
  size_t boxSize = (static_cast<size_t>(bytes[0]) << 24) | (static_cast<size_t>(bytes[1]) << 16) |
                   (static_cast<size_t>(bytes[2]) << 8) | static_cast<size_t>(bytes[3]);
  // A zero box size means "to the end of the file" per ISO-BMFF.
  size_t limit = (boxSize == 0 || boxSize > size) ? size : boxSize;
  for (size_t offset = 16; offset + 4 <= limit; offset += 4) {
    if (MatchesTag(bytes + offset, brand)) {
      return true;
    }
  }
  return false;
}

// SVG is XML text, so it carries no magic bytes: after an optional UTF-8 BOM, a leading `<?xml`
// that mentions `<svg` nearby, or a leading `<svg`, is the same signal the html-snapshot
// downloader uses to label an inlined icon.
bool LooksLikeSvg(const uint8_t* bytes, size_t size) {
  size_t offset = 0;
  if (size >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF) {
    offset = 3;
  }
  while (offset < size && (bytes[offset] == ' ' || bytes[offset] == '\t' || bytes[offset] == '\r' ||
                           bytes[offset] == '\n')) {
    offset++;
  }
  if (offset + 4 <= size && MatchesTag(bytes + offset, "<svg")) {
    return true;
  }
  if (offset + 5 > size || bytes[offset] != '<' || bytes[offset + 1] != '?' ||
      bytes[offset + 2] != 'x' || bytes[offset + 3] != 'm' || bytes[offset + 4] != 'l') {
    return false;
  }
  size_t probeEnd = offset + 256 < size ? offset + 256 : size;
  for (size_t i = offset; i + 4 <= probeEnd; i++) {
    if (MatchesTag(bytes + i, "<svg")) {
      return true;
    }
  }
  return false;
}

}  // namespace

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
  if (size >= 4 && MatchesTag(bytes, "RIFF")) {
    return "image/webp";
  }
  // GIF: "GIF87a" or "GIF89a"
  if (size >= 4 && MatchesTag(bytes, "GIF8")) {
    return "image/gif";
  }
  // AVIF / HEIC: both are ISO-BMFF containers distinguished only by their brand.
  if (HasIsoBmffBrand(bytes, size, "avif") || HasIsoBmffBrand(bytes, size, "avis")) {
    return "image/avif";
  }
  if (HasIsoBmffBrand(bytes, size, "heic") || HasIsoBmffBrand(bytes, size, "heix") ||
      HasIsoBmffBrand(bytes, size, "hevc") || HasIsoBmffBrand(bytes, size, "hevx") ||
      HasIsoBmffBrand(bytes, size, "heim") || HasIsoBmffBrand(bytes, size, "heis") ||
      HasIsoBmffBrand(bytes, size, "mif1") || HasIsoBmffBrand(bytes, size, "msf1")) {
    return "image/heic";
  }
  if (LooksLikeSvg(bytes, size)) {
    return "image/svg+xml";
  }
  return nullptr;
}

bool IsSupportedImageMime(const char* mime) {
  if (mime == nullptr) {
    return false;
  }
  const char* supported[] = {"image/png", "image/jpeg", "image/webp", "image/gif"};
  for (const char* candidate : supported) {
    size_t i = 0;
    for (; mime[i] != '\0' && candidate[i] != '\0'; i++) {
      if (mime[i] != candidate[i]) {
        break;
      }
    }
    if (mime[i] == '\0' && candidate[i] == '\0') {
      return true;
    }
  }
  return false;
}

}  // namespace pagx
