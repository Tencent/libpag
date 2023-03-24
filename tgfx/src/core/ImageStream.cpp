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

#include "core/ImageStream.h"
#include "tgfx/core/ImageReader.h"

namespace tgfx {
void ImageStream::markContentDirty(const Rect& bounds) {
  std::lock_guard<std::mutex> autoLock(locker);
  for (auto& reader : readers) {
    reader->onContentDirty(bounds);
  }
}

void ImageStream::attachToStream(ImageReader* imageReader) {
  std::lock_guard<std::mutex> autoLock(locker);
  readers.push_back(imageReader);
}

void ImageStream::detachFromStream(ImageReader* imageReader) {
  std::lock_guard<std::mutex> autoLock(locker);
  auto result = std::find(readers.begin(), readers.end(), imageReader);
  if (result != readers.end()) {
    readers.erase(result);
  }
}
}  // namespace tgfx
