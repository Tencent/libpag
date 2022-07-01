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

#include "ResourceHandle.h"
#include "tgfx/core/Matrix.h"

namespace tgfx {
class ProgramDataManager {
 public:
  virtual ~ProgramDataManager() = default;

  virtual void set1f(UniformHandle handle, float v0) const = 0;

  virtual void set2f(UniformHandle handle, float v0, float v1) const = 0;

  virtual void set4f(UniformHandle handle, float v0, float v1, float v2, float v3) const = 0;

  virtual void set4fv(UniformHandle handle, int arrayCount, const float v[]) const = 0;

  virtual void setMatrix3f(UniformHandle handle, const float matrix[]) const = 0;

  virtual void setMatrix4f(UniformHandle handle, const float matrix[]) const = 0;

  // convenience method for uploading a Matrix to a 3x3 matrix uniform
  virtual void setMatrix(UniformHandle u, const Matrix& matrix) const = 0;
};
}  // namespace tgfx
