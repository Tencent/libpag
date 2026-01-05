/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "ScopedLock.h"

namespace pag {
ScopedLock::ScopedLock(std::shared_ptr<std::mutex> first, std::shared_ptr<std::mutex> second)
    : firstLocker(std::move(first)), secondLocker(std::move(second)) {
  if (firstLocker == nullptr) {
    return;
  }
  if (firstLocker == secondLocker) {
    secondLocker = nullptr;
  }
  if (secondLocker) {
    std::lock(*firstLocker, *secondLocker);
  } else {
    firstLocker->lock();
  }
}

ScopedLock::~ScopedLock() {
  if (firstLocker) {
    firstLocker->unlock();
  }
  if (secondLocker) {
    secondLocker->unlock();
  }
}
}  // namespace pag
