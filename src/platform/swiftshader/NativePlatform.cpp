/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "NativePlatform.h"
#include <ft2build.h>
#include <atomic>
#include <fstream>
#include "image/Image.h"
#include "image/PixelMap.h"
#include "pag/pag.h"
#include FT_ADVANCES_H
#include FT_MODULE_H

namespace pag {
typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_* FT_Face;

extern "C" {
static void* ft_alloc(FT_Memory, long size) {
  return malloc(static_cast<size_t>(size));
}
static void ft_free(FT_Memory, void* block) {
  free(block);
}
static void* ft_realloc(FT_Memory, long, long new_size, void* block) {
  return realloc(block, static_cast<size_t>(new_size));
}
}

static FT_MemoryRec_ gFTMemory = {nullptr, ft_alloc, ft_free, ft_realloc};
static std::atomic<NALUType> defaultType = {NALUType::AnnexB};

const Platform* Platform::Current() {
  static const NativePlatform platform = {};
  return &platform;
}

NALUType NativePlatform::naluType() const {
  return defaultType;
}

void NativePlatform::setNALUType(NALUType type) const {
  defaultType = type;
}

static PAGFont ParseFont(const FT_Open_Args* args, int ttcIndex) {
  if (args == nullptr) {
    return {"", ""};
  }
  FT_Library fLibrary = nullptr;
  if (FT_New_Library(&gFTMemory, &fLibrary)) {
    return {"", ""};
  }
  FT_Add_Default_Modules(fLibrary);
  FT_Face face;
  if (FT_Open_Face(fLibrary, args, ttcIndex, &face)) {
    FT_Done_Library(fLibrary);
    return {"", ""};
  }
  if (face->family_name == nullptr) {
    return {"", ""};
  }
  std::string fontFamily = face->family_name;
  std::string fontStyle = face->style_name;
  FT_Done_Face(face);
  FT_Done_Library(fLibrary);
  return {fontFamily, fontStyle};
}

PAGFont NativePlatform::parseFont(const std::string& fontPath, int ttcIndex) const {
  if (fontPath.empty()) {
    return {"", ""};
  }
  FT_Open_Args args = {};
  memset(&args, 0, sizeof(args));
  args.flags = FT_OPEN_PATHNAME;
  args.pathname = const_cast<FT_String*>(fontPath.c_str());
  return ParseFont(&args, ttcIndex);
}

PAGFont NativePlatform::parseFont(const void* data, size_t length, int ttcIndex) const {
  if (length == 0) {
    return {"", ""};
  }
  FT_Open_Args args = {};
  memset(&args, 0, sizeof(args));
  args.flags = FT_OPEN_MEMORY;
  args.memory_base = (const FT_Byte*)data;
  args.memory_size = static_cast<FT_Long>(length);
  return ParseFont(&args, ttcIndex);
}

void NativePlatform::traceImage(const PixelMap& pixelMap, const std::string& tag) const {
  std::string path = tag;
  if (path.empty()) {
    path = "TraceImage.png";
  } else if (path.rfind(".png") != path.size() - 4 && path.rfind(".PNG") != path.size() - 4) {
    path += ".png";
  }
  auto bytes = Image::Encode(pixelMap.info(), pixelMap.pixels(), EncodedFormat::PNG, 100);
  if (bytes) {
    std::ofstream out(path);
    out.write(reinterpret_cast<const char*>(bytes->data()), bytes->size());
    out.close();
  }
}
}  // namespace pag