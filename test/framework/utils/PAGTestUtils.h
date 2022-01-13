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

#include "image/Bitmap.h"
#include "image/PixelBuffer.h"
#include "image/PixelMap.h"
#include "pag/pag.h"

namespace pag {
std::string DumpMD5(const void* bytes, size_t size);

std::string DumpMD5(const Bitmap& bitmap);

std::string DumpMD5(const PixelMap& pixelMap);

std::string DumpMD5(std::shared_ptr<PixelBuffer> pixelBuffer);

std::string DumpMD5(std::shared_ptr<PAGSurface> pagSurface);

Bitmap MakeSnapshot(std::shared_ptr<PAGSurface> pagSurface);

void TraceIf(const Bitmap& bitmap, const std::string& path, bool condition);

void TraceIf(std::shared_ptr<PixelBuffer> pixelBuffer, const std::string& path, bool condition);

void TraceIf(const PixelMap& pixelMap, const std::string& path, bool condition);

void TraceIf(std::shared_ptr<PAGSurface> pagSurface, const std::string& path, bool condition);

std::shared_ptr<PAGLayer> GetLayer(std::shared_ptr<PAGComposition> root, LayerType type,
                                   int& targetIndex);
}  // namespace pag
