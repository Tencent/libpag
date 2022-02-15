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

#include "PorterDuffXferProcessor.h"
#include "base/utils/UniqueID.h"
#include "opengl/GLPorterDuffXferProcessor.h"

namespace pag {
std::unique_ptr<PorterDuffXferProcessor> PorterDuffXferProcessor::Make(Blend blend) {
  return std::unique_ptr<PorterDuffXferProcessor>(new PorterDuffXferProcessor(blend));
}

void PorterDuffXferProcessor::computeProcessorKey(Context*, BytesKey* bytesKey) const {
  static auto Type = UniqueID::Next();
  bytesKey->write(Type);
  bytesKey->write(static_cast<uint32_t>(blend));
}

std::unique_ptr<GLXferProcessor> PorterDuffXferProcessor::createGLInstance() const {
  return std::make_unique<GLPorterDuffXferProcessor>();
}
}  // namespace pag
