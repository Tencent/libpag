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

#include <list>
#include <unordered_map>

#include "tgfx/core/BytesKey.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/PixelBuffer.h"
#include "tgfx/core/Pixmap.h"

namespace tgfx {
class GradientCache {
 public:
  std::shared_ptr<Texture> getGradient(Context* context, const Color* colors,
                                       const float* positions, int count);

  void releaseAll();

  bool empty() const;

 private:
  std::shared_ptr<Texture> find(const BytesKey& bytesKey);

  void add(const BytesKey& bytesKey, std::shared_ptr<Texture> texture);

  std::list<BytesKey> keys = {};
  std::unordered_map<BytesKey, std::shared_ptr<Texture>, BytesHasher> textures = {};
};
}  // namespace tgfx
