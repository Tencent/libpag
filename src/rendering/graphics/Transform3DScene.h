/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "Graphic.h"
#include "pag/file.h"
#include "rendering/filters/Transform3DFilter.h"

namespace pag {
class PAGComposition;

struct Transform3DSceneLayer {
  std::shared_ptr<Graphic> graphic;
  Transform3DDepthPlane depthPlane;
  BlendMode blendMode = BlendMode::Normal;
};

/** 返回合成当前缓存的三维场景图形，供测试核验复用与失效；无缓存时返回空。 */
std::shared_ptr<Graphic> GetCached3DGraphic(const PAGComposition* composition);

class Transform3DSceneBuilder {
 public:
  Transform3DSceneBuilder(int width, int height) : width(width), height(height) {
  }

  /** 添加图层，二维内容分隔连续三维场景，并保留背景混合关系。 */
  void addLayer(std::shared_ptr<Graphic> graphic, Layer* layer, Frame frame,
                const std::shared_ptr<Transform3DContext>& context,
                const tgfx::Matrix& extraMatrix);

  /** 结束最后一个三维场景并返回合成内容。 */
  std::shared_ptr<Graphic> makeGraphic();

 private:
  void flushScene();
  int width = 0;
  int height = 0;
  std::vector<std::shared_ptr<Graphic>> background;
  std::vector<Transform3DSceneLayer> layers;
};

}  // namespace pag
