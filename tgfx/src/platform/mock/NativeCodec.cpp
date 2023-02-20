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

#include <CoreGraphics/CGBitmapContext.h>
#include "platform/NativeCodec.h"

namespace tgfx {
std::shared_ptr<ImageCodec> NativeCodec::MakeCodec(const std::string&) {
  return nullptr;
}

std::shared_ptr<ImageCodec> NativeCodec::MakeCodec(std::shared_ptr<Data>) {
  return nullptr;
}

std::shared_ptr<ImageCodec> NativeCodec::MakeFrom(void* /*nativeImage*/) {
  return nullptr;
}

CGContextRef CreateBitmapContext(const ImageInfo&, void*) {
  return nullptr;
}
}  // namespace tgfx
