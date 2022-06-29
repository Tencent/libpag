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

#include "GeometryProcessor.h"

namespace tgfx {
class DefaultGeometryProcessor : public GeometryProcessor {
 public:
  static std::unique_ptr<DefaultGeometryProcessor> Make(int width, int height,
                                                        const Matrix& localMatrix);

  std::string name() const override {
    return "DefaultGeometryProcessor";
  }

  std::unique_ptr<GLGeometryProcessor> createGLInstance() const override;

 private:
  DefaultGeometryProcessor(int width, int height, const Matrix& localMatrix);

  void onComputeProcessorKey(BytesKey* bytesKey) const override;

  Attribute position;
  Attribute coverage;

  int width = 1;
  int height = 1;
  Matrix localMatrix = Matrix::I();

  friend class GLDefaultGeometryProcessor;
};
}  // namespace tgfx
