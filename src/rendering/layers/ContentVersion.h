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

#pragma once

#include "pag/pag.h"
#include "rendering/utils/LockGuard.h"

namespace pag {
class ContentVersion {
 public:
  static uint32_t Get(std::shared_ptr<PAGLayer> pagLayer) {
    if (pagLayer == nullptr) {
      return 0;
    }
    LockGuard autoLock(pagLayer->rootLocker);
    return pagLayer->contentVersion;
  }

  static bool CheckFrameChanged(std::shared_ptr<PAGDecoder> decoder, int index) {
    if (decoder == nullptr) {
      return false;
    }
    return decoder->checkFrameChanged(index);
  }
};
}  // namespace pag
