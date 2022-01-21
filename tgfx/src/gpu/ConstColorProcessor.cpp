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

#include "ConstColorProcessor.h"
#include "opengl/GLConstColorProcessor.h"

namespace pag {
std::unique_ptr<ConstColorProcessor> ConstColorProcessor::Make(Color4f color) {
  return std::unique_ptr<ConstColorProcessor>(new ConstColorProcessor(color));
}

void ConstColorProcessor::onComputeProcessorKey(BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
  uint32_t flag = color.a != 1.0f ? 1 : 0;
  bytesKey->write(flag);
}

std::unique_ptr<GLFragmentProcessor> ConstColorProcessor::onCreateGLInstance() const {
  return std::make_unique<GLConstColorProcessor>();
}
}  // namespace pag
