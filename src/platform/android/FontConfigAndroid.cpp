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

#include "FontConfigAndroid.h"
#include <ft2build.h>
#include "JNIEnvironment.h"
#include "JNIHelper.h"
#include FT_ADVANCES_H
#include FT_MODULE_H

namespace pag {
typedef struct FT_LibraryRec_* FT_Library;
typedef struct FT_FaceRec_* FT_Face;

static Global<jclass> PAGFontClass;
static jmethodID PAGFont_RegisterFallbackFonts;

extern "C" {

void FontConfigAndroid::InitJNI(JNIEnv* env) {
  PAGFontClass.reset(env, env->FindClass("org/libpag/PAGFont"));
  if (PAGFontClass.get() == nullptr) {
    LOGE("Could not run PAGFont.RegisterFallbackFonts(), class is not found!");
    return;
  }
  PAGFont_RegisterFallbackFonts =
      env->GetStaticMethodID(PAGFontClass.get(), "RegisterFallbackFonts", "()V");
}

bool FontConfigAndroid::RegisterFallbackFonts() {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return false;
  }
  if (PAGFontClass.get() == nullptr) {
    LOGE("PAGFontClass is null");
    return false;
  }
  env->CallStaticVoidMethod(PAGFontClass.get(), PAGFont_RegisterFallbackFonts);
  return true;
}

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

PAGFont FontConfigAndroid::Parse(const void* data, size_t length, int ttcIndex) {
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

PAGFont FontConfigAndroid::Parse(const std::string& fontPath, int ttcIndex) {
  if (fontPath.empty()) {
    return {"", ""};
  }
  FT_Open_Args args = {};
  memset(&args, 0, sizeof(args));
  args.flags = FT_OPEN_PATHNAME;
  args.pathname = const_cast<FT_String*>(fontPath.c_str());
  return ParseFont(&args, ttcIndex);
}
}  // namespace pag
