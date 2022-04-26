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
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Clock.h"
#include "tgfx/core/PixelBuffer.h"

namespace pag {
std::string ToString(Frame frame);

void GetAllPAGFiles(std::string path, std::vector<std::string>& files);

std::shared_ptr<tgfx::PixelBuffer> MakeSnapshot(std::shared_ptr<PAGSurface> pagSurface);

std::shared_ptr<PAGLayer> GetLayer(std::shared_ptr<PAGComposition> root, LayerType type,
                                   int& targetIndex);

bool CreateGLTexture(tgfx::Context* context, int width, int height, tgfx::GLSampler* texture);
}  // namespace pag
