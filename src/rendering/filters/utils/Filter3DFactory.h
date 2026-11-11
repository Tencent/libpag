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

#include "tgfx/core/Image.h"
#include "tgfx/core/Point.h"
#include "tgfx/core/Rect.h"
#include "tgfx/gpu/GPU.h"

namespace pag {
class Layer;
class RenderCache;
struct FilterList;

std::shared_ptr<tgfx::Image> Apply3DEffects(std::shared_ptr<tgfx::Image> input, RenderCache* cache,
                                            const FilterList* filterList,
                                            const tgfx::Rect& clipBounds,
                                            const tgfx::Point& sourceScale,
                                            tgfx::Rect* filterBounds, tgfx::Point* offset);

/** 判断 GPU 是否支持三维投影与逐像素透明合成所需的格式和资源限制。 */
bool Has3DSupport(const tgfx::GPU* gpu);

/** 将三维内容采样比例限制在 GPU 纹理范围内，无效输入返回零。 */
float Clamp3DScale(const tgfx::GPU* gpu, float width, float height, float scale);

/** 检查图层自身或父链是否包含三维变换。 */
bool Has3DTransform(const Layer* layer);

}  // namespace pag
