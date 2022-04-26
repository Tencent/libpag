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
#include "GLDrawer.h"
#include "tgfx/core/Path.h"

namespace tgfx {
class GLRRectOp : public GLDrawOp {
 public:
  static std::unique_ptr<GLRRectOp> Make(const RRect& rRect, const Matrix& viewMatrix);

  std::unique_ptr<GeometryProcessor> getGeometryProcessor(const DrawArgs& args) override;

  std::vector<float> vertices(const DrawArgs& args) override;

  std::shared_ptr<GLBuffer> getIndexBuffer(const DrawArgs& args) override;

 private:
  GLRRectOp(const RRect& rRect, const Matrix& viewMatrix);

  RRect rRect;
  Matrix viewMatrix = Matrix::I();
  float xRadius = 0;
  float yRadius = 0;
  float innerXRadius = 0;
  float innerYRadius = 0;
  //  bool stroked = false;
  //  Point strokeWidths = Point::Zero();
};
}  // namespace tgfx
