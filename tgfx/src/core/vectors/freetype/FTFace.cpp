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

#include "FTFace.h"
#include "FTUtil.h"

namespace pag {
std::mutex& FTMutex() {
  static std::mutex& mutex = *new std::mutex;
  return mutex;
}

static FTLibrary* gFTLibrary;
static int gFTCount = 0;

static bool RefFTLibrary() {
  if (0 == gFTCount) {
    gFTLibrary = new FTLibrary;
  }
  ++gFTCount;
  return gFTLibrary->library() != nullptr;
}

// Caller must lock f_t_mutex() before calling this function.
static void UnrefFTLibrary() {
  --gFTCount;
  if (0 == gFTCount) {
    delete gFTLibrary;
    gFTLibrary = nullptr;
  }
}

FTFace::FTFace() {
  RefFTLibrary();
}

FTFace::~FTFace() {
  if (face) {
    FT_Done_Face(face);
  }
  UnrefFTLibrary();
}

std::unique_ptr<FTFace> FTFace::Make(const FTFontData& data) {
  std::lock_guard<std::mutex> lockGuard(FTMutex());
  auto face = std::make_unique<FTFace>();
  FT_Open_Args args;
  memset(&args, 0, sizeof(args));
  if (data.path.empty()) {
    args.flags = FT_OPEN_MEMORY;
    args.memory_base = static_cast<const FT_Byte*>(data.data->data());
    args.memory_size = static_cast<FT_Long>(data.data->length());
  } else {
    args.flags = FT_OPEN_PATHNAME;
    args.pathname = const_cast<FT_String*>(data.path.c_str());
  }
  auto err = FT_Open_Face(gFTLibrary->library(), &args, data.ttcIndex, &face->face);
  if (err) {
    return nullptr;
  }
  if (!face->face->charmap) {
    FT_Select_Charmap(face->face, FT_ENCODING_MS_SYMBOL);
  }
  return face;
}
}  // namespace pag
