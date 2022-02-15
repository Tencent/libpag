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

#include "SingleIntervalGradientColorizer.h"
#include "base/utils/UniqueID.h"
#include "gpu/opengl/GLSingleIntervalGradientColorizer.h"

namespace pag {
std::unique_ptr<SingleIntervalGradientColorizer> SingleIntervalGradientColorizer::Make(
    Color4f start, Color4f end) {
  return std::unique_ptr<SingleIntervalGradientColorizer>(
      new SingleIntervalGradientColorizer(start, end));
}

void SingleIntervalGradientColorizer::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
}

std::unique_ptr<GLFragmentProcessor> SingleIntervalGradientColorizer::onCreateGLInstance() const {
  return std::make_unique<GLSingleIntervalGradientColorizer>();
}
}  // namespace pag
