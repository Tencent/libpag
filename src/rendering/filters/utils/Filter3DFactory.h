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

/**
 * 将三维内容采样比例限制在 GPU 纹理范围与显存预算内，无效输入返回零。
 * memoryBudget 为显存预算字节数（通常取 Context 的缓存上限），textureCount 为目标分辨率下
 * 需同时持有的每像素 4 字节全幅纹理数；超预算时按面积比降采样而非直接失败。
 */
float Clamp3DScale(const tgfx::GPU* gpu, size_t memoryBudget, int textureCount, float width,
                   float height, float scale);

/** 检查图层自身或父链是否包含三维变换。 */
bool Has3DTransform(const Layer* layer);

/**
 * 当前 GPU 后端的裁剪空间 Y 轴是否相对左上角内容约定翻转，需要在投影前做竖直翻转。
 * 集中封装后端差异，避免 Y 轴约定散落在多处矩阵与顶点计算里。
 */
bool NeedsClipSpaceYFlip();

}  // namespace pag
