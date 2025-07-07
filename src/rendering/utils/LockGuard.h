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

#pragma once

#include <memory>
#include <mutex>

namespace pag {

class LockGuard {
 public:
  explicit LockGuard(std::shared_ptr<std::mutex> locker) : mutex(std::move(locker)) {
    if (mutex) {
      mutex->lock();
    }
  }

  ~LockGuard() {
    if (mutex) {
      mutex->unlock();
    }
  }

 private:
  std::shared_ptr<std::mutex> mutex;
};

}  // namespace pag
