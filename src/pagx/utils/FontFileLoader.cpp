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

#include "pagx/utils/FontFileLoader.h"
#include <woff2/decode.h>
#include <woff2/output.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include "pagx/FontConfig.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Typeface.h"

namespace pagx {

// Bounds the face count a malformed collection header may claim; real collections hold far fewer
// faces.
static constexpr int MAX_FONT_FACES = 64;

static bool HasWOFF2Signature(const void* bytes, size_t length) {
  return bytes != nullptr && length >= 4 && std::memcmp(bytes, "wOF2", 4) == 0;
}

// The leading bytes of a file: enough for the WOFF2 signature and a collection's face count.
static std::string ReadFileHeader(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  char header[12] = {};
  file.read(header, sizeof(header));
  return std::string(header, static_cast<size_t>(file.gcount()));
}

static bool FileHasWOFF2Signature(const std::string& path) {
  auto header = ReadFileHeader(path);
  return HasWOFF2Signature(header.data(), header.size());
}

static void ReleaseDecodedFont(const void*, void* context) {
  delete static_cast<std::string*>(context);
}

static std::shared_ptr<tgfx::Data> DecodeWOFF2(const void* bytes, size_t length) {
  auto data = static_cast<const uint8_t*>(bytes);
  auto sfnt = std::make_unique<std::string>();
  woff2::WOFF2StringOut out(sfnt.get());
  // The header's totalSfntSize is only a hint: reconstructed glyf/loca tables may differ from the
  // original, so it merely reserves capacity while the writer grows up to its own size cap.
  sfnt->reserve(std::min(woff2::ComputeWOFF2FinalSize(data, length), out.MaxSize()));
  if (!woff2::ConvertWOFF2ToTTF(data, length, &out) || out.Size() == 0) {
    return nullptr;
  }
  sfnt->resize(out.Size());
  auto* decoded = sfnt.release();
  return tgfx::Data::MakeAdopted(decoded->data(), decoded->size(), ReleaseDecodedFont, decoded);
}

static std::shared_ptr<tgfx::Data> DecodeWOFF2File(const std::string& path) {
  auto file = tgfx::Data::MakeFromFile(path);
  if (file == nullptr) {
    return nullptr;
  }
  return DecodeWOFF2(file->data(), file->size());
}

std::shared_ptr<tgfx::Typeface> MakeTypefaceFromFile(const std::string& path, int ttcIndex) {
  if (!FileHasWOFF2Signature(path)) {
    return tgfx::Typeface::MakeFromPath(path, ttcIndex);
  }
  auto sfnt = DecodeWOFF2File(path);
  return sfnt != nullptr ? tgfx::Typeface::MakeFromData(std::move(sfnt), ttcIndex) : nullptr;
}

std::shared_ptr<tgfx::Typeface> MakeTypefaceFromBytes(const void* bytes, size_t length,
                                                      int ttcIndex) {
  if (!HasWOFF2Signature(bytes, length)) {
    return tgfx::Typeface::MakeFromBytes(bytes, length, ttcIndex);
  }
  auto sfnt = DecodeWOFF2(bytes, length);
  return sfnt != nullptr ? tgfx::Typeface::MakeFromData(std::move(sfnt), ttcIndex) : nullptr;
}

int CountFontFaces(const void* bytes, size_t length) {
  auto data = static_cast<const uint8_t*>(bytes);
  if (data == nullptr || length < 4) {
    return 0;
  }
  if (std::memcmp(data, "ttcf", 4) != 0) {
    return 1;
  }
  if (length < 12) {
    return 0;
  }
  auto numFonts = (static_cast<uint32_t>(data[8]) << 24) | (static_cast<uint32_t>(data[9]) << 16) |
                  (static_cast<uint32_t>(data[10]) << 8) | static_cast<uint32_t>(data[11]);
  return static_cast<int>(std::min(numFonts, static_cast<uint32_t>(MAX_FONT_FACES)));
}

static std::vector<std::shared_ptr<tgfx::Typeface>> MakeTypefacesFromSFNT(
    const std::shared_ptr<tgfx::Data>& sfnt) {
  std::vector<std::shared_ptr<tgfx::Typeface>> faces = {};
  auto faceCount = CountFontFaces(sfnt->data(), sfnt->size());
  for (int index = 0; index < faceCount; index++) {
    auto typeface = tgfx::Typeface::MakeFromData(sfnt, index);
    if (typeface == nullptr) {
      break;
    }
    faces.push_back(std::move(typeface));
  }
  return faces;
}

std::vector<std::shared_ptr<tgfx::Typeface>> MakeTypefacesFromFile(const std::string& path) {
  auto header = ReadFileHeader(path);
  if (HasWOFF2Signature(header.data(), header.size())) {
    auto sfnt = DecodeWOFF2File(path);
    return sfnt != nullptr ? MakeTypefacesFromSFNT(sfnt)
                           : std::vector<std::shared_ptr<tgfx::Typeface>>{};
  }
  std::vector<std::shared_ptr<tgfx::Typeface>> faces = {};
  auto faceCount = CountFontFaces(header.data(), header.size());
  for (int index = 0; index < faceCount; index++) {
    auto typeface = tgfx::Typeface::MakeFromPath(path, index);
    if (typeface == nullptr) {
      break;
    }
    faces.push_back(std::move(typeface));
  }
  return faces;
}

std::vector<std::shared_ptr<tgfx::Typeface>> MakeTypefacesFromBytes(const void* bytes,
                                                                    size_t length) {
  std::shared_ptr<tgfx::Data> sfnt = nullptr;
  if (HasWOFF2Signature(bytes, length)) {
    sfnt = DecodeWOFF2(bytes, length);
  } else if (bytes != nullptr && length > 0) {
    sfnt = tgfx::Data::MakeWithCopy(bytes, length);
  }
  return sfnt != nullptr ? MakeTypefacesFromSFNT(sfnt)
                         : std::vector<std::shared_ptr<tgfx::Typeface>>{};
}

int RegisterFontDataFaces(FontConfig* fontConfig, const void* bytes, size_t length,
                          bool addToFallbackChain, const std::string& familyOverride) {
  // The font is copied (or decoded) once and the same buffer backs every face's registration and
  // fallback entry: taking the bytes overloads per face would hold one full copy per face per list.
  std::shared_ptr<const std::vector<uint8_t>> buffer = nullptr;
  if (HasWOFF2Signature(bytes, length)) {
    auto sfnt = DecodeWOFF2(bytes, length);
    if (sfnt != nullptr && sfnt->size() > 0) {
      auto* decoded = static_cast<const uint8_t*>(sfnt->data());
      buffer = std::make_shared<const std::vector<uint8_t>>(decoded, decoded + sfnt->size());
    }
  } else if (bytes != nullptr && length > 0) {
    auto* data = static_cast<const uint8_t*>(bytes);
    buffer = std::make_shared<const std::vector<uint8_t>>(data, data + length);
  }
  if (buffer == nullptr || buffer->empty()) {
    return 0;
  }
  // Faces are enumerated through a no-copy view: the typefaces built here only report their
  // family and style, and the buffer itself is owned by the registrations that follow.
  auto sfnt = tgfx::Data::MakeWithoutCopy(buffer->data(), buffer->size());
  int faceCount = 0;
  auto faceLimit = CountFontFaces(buffer->data(), buffer->size());
  for (int index = 0; index < faceLimit; index++) {
    auto typeface = tgfx::Typeface::MakeFromData(sfnt, index);
    if (typeface == nullptr) {
      break;
    }
    auto family = familyOverride.empty() ? typeface->fontFamily() : familyOverride;
    auto style = typeface->fontStyle();
    fontConfig->registerFont(buffer, index, family, style);
    if (addToFallbackChain) {
      fontConfig->addFallbackFont(buffer, index, family, style);
    }
    faceCount++;
  }
  return faceCount;
}

int RegisterFontFileFaces(FontConfig* fontConfig, const std::string& path, bool addToFallbackChain,
                          const std::string& familyOverride, std::string* internalFamily) {
  auto faces = MakeTypefacesFromFile(path);
  for (size_t index = 0; index < faces.size(); index++) {
    auto ttcIndex = static_cast<int>(index);
    if (index == 0 && internalFamily != nullptr) {
      *internalFamily = faces[index]->fontFamily();
    }
    auto family = familyOverride.empty() ? faces[index]->fontFamily() : familyOverride;
    auto style = faces[index]->fontStyle();
    fontConfig->registerFont(path, ttcIndex, family, style);
    if (addToFallbackChain) {
      fontConfig->addFallbackFont(path, ttcIndex, family, style);
    }
  }
  return static_cast<int>(faces.size());
}

}  // namespace pagx
