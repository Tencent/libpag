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

#pragma once

#include "Baseline.h"
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/utils/Clock.h"

namespace pag {
std::string ToString(Frame frame);

BackendTexture ToBackendTexture(const tgfx::GLTextureInfo& texture, int width, int height);

void GetAllPAGFiles(const std::string& path, std::vector<std::string>& files);

tgfx::Bitmap MakeSnapshot(std::shared_ptr<PAGSurface> pagSurface);

std::shared_ptr<PAGLayer> GetLayer(std::shared_ptr<PAGComposition> root, LayerType type,
                                   int& targetIndex);

bool CreateGLTexture(tgfx::Context* context, int width, int height, tgfx::GLTextureInfo* texture);

std::shared_ptr<PAGFile> LoadPAGFile(const std::string& path);

std::shared_ptr<tgfx::ImageCodec> MakeImageCodec(const std::string& path);

std::shared_ptr<tgfx::Image> MakeImage(const std::string& path);

std::shared_ptr<PAGImage> MakePAGImage(const std::string& path);

void SaveFile(std::shared_ptr<tgfx::Data> data, const std::string& key);

void SaveImage(const std::shared_ptr<tgfx::PixelBuffer> pixelBuffer, const std::string& key);

void SaveImage(const tgfx::Bitmap& bitmap, const std::string& key);

void SaveImage(const tgfx::Pixmap& pixmap, const std::string& key);

void RemoveImage(const std::string& key);
}  // namespace pag
