/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "UniformBuffer.h"

namespace tgfx {
void UniformBuffer::setMatrix(const std::string& name, const tgfx::Matrix& matrix) {
  float values[6];
  matrix.get6(values);
  float data[] = {values[0], values[3], 0, values[1], values[4], 0, values[2], values[5], 1};
  setData(name, data);
}
}  // namespace tgfx