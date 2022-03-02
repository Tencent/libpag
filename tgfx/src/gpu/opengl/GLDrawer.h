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

#include "GLBuffer.h"
#include "GLContext.h"
#include "core/BlendMode.h"
#include "gpu/AAType.h"
#include "gpu/FragmentProcessor.h"
#include "gpu/GeometryProcessor.h"
#include "gpu/opengl/GLRenderTarget.h"

namespace tgfx {
struct DrawArgs {
  Context* context = nullptr;
  BlendMode blendMode = BlendMode::SrcOver;
  Matrix viewMatrix = Matrix::I();
  const RenderTarget* renderTarget = nullptr;
  std::shared_ptr<Texture> renderTargetTexture = nullptr;
  Rect rectToDraw = Rect::MakeEmpty();
  Rect scissorRect = Rect::MakeEmpty();
  AAType aa = AAType::None;
  std::vector<std::unique_ptr<FragmentProcessor>> colors;
  std::vector<std::unique_ptr<FragmentProcessor>> masks;
};

class GLDrawOp {
 public:
  virtual ~GLDrawOp() = default;

  virtual std::unique_ptr<GeometryProcessor> getGeometryProcessor(const DrawArgs& args) = 0;

  virtual std::vector<float> vertices(const DrawArgs& args) = 0;

  virtual std::shared_ptr<GLBuffer> getIndexBuffer(const DrawArgs& args) = 0;
};

class GLDrawer : public Resource {
 public:
  static std::shared_ptr<GLDrawer> Make(Context* context);

  void draw(DrawArgs args, std::unique_ptr<GLDrawOp> op) const;

 protected:
  void computeRecycleKey(BytesKey*) const override;

 private:
  bool init(Context* context);

  void onReleaseGPU() override;

  unsigned vertexArray = 0;
  unsigned vertexBuffer = 0;
};
}  // namespace tgfx
