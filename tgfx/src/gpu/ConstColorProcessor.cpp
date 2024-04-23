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

#include "gpu/ConstColorProcessor.h"
#include "opengl/GLConstColorProcessor.h"

namespace tgfx {
std::unique_ptr<ConstColorProcessor> ConstColorProcessor::Make(Color color, InputMode mode) {
  return std::unique_ptr<ConstColorProcessor>(new ConstColorProcessor(color, mode));
}

void ConstColorProcessor::onComputeProcessorKey(BytesKey* bytesKey) const {
  bytesKey->write(static_cast<uint32_t>(inputMode));
}

bool ConstColorProcessor::onIsEqual(const FragmentProcessor& processor) const {
  const auto& that = static_cast<const ConstColorProcessor&>(processor);
  return inputMode == that.inputMode && color == that.color;
}

std::unique_ptr<GLFragmentProcessor> ConstColorProcessor::onCreateGLInstance() const {
  return std::make_unique<GLConstColorProcessor>();
}
}  // namespace tgfx