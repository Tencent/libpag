/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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
#include "base/PAGTest.h"
#include "base/utils/TGFXCast.h"
#include "pag/pag.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/Image.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"
#include "utils/Baseline.h"
#include "utils/OffscreenSurface.h"
#include "utils/ProjectPath.h"
#include "utils/Semaphore.h"

#ifdef TGFX_USE_OPENGL
#include "tgfx/gpu/opengl/GLTypes.h"
#endif

#ifdef TGFX_USE_METAL
#include "tgfx/gpu/metal/MetalTypes.h"
#endif

namespace pag {
std::string ToString(Frame frame);

#ifdef TGFX_USE_OPENGL
BackendTexture ToBackendTexture(const tgfx::GLTextureInfo& texture, int width, int height);
bool CreateGLTexture(tgfx::Context* context, int width, int height, tgfx::GLTextureInfo* texture);
#endif

#ifdef TGFX_USE_METAL
// Backend-specific helpers for Metal tests. Implemented in TestUtils_Metal.mm (Objective-C++ is
// required to bridge the id<MTLTexture> stored in MetalTextureInfo::texture).
BackendTexture ToBackendTexture(const tgfx::MetalTextureInfo& texture, int width, int height);
BackendRenderTarget ToBackendRenderTarget(const tgfx::MetalTextureInfo& texture, int width,
                                          int height);
bool CreateMetalTexture(tgfx::Context* context, int width, int height,
                        tgfx::MetalTextureInfo* texture);
void ReleaseMetalTexture(tgfx::MetalTextureInfo* texture);
#endif

std::vector<std::string> GetAllPAGFiles(const std::string& path);

tgfx::Bitmap MakeSnapshot(std::shared_ptr<PAGSurface> pagSurface);

std::shared_ptr<PAGLayer> GetLayer(std::shared_ptr<PAGComposition> root, LayerType type,
                                   int& targetIndex);

std::shared_ptr<PAGFile> LoadPAGFile(const std::string& path);

std::shared_ptr<tgfx::ImageCodec> MakeImageCodec(const std::string& path);

std::shared_ptr<tgfx::Image> MakeImage(const std::string& path);

std::shared_ptr<PAGImage> MakePAGImage(const std::string& path);

std::shared_ptr<tgfx::Data> ReadFile(const std::string& path);

void SaveImage(const tgfx::Bitmap& bitmap, const std::string& key);

void SaveImage(const tgfx::Pixmap& pixmap, const std::string& key);

void RemoveImage(const std::string& key);
}  // namespace pag
