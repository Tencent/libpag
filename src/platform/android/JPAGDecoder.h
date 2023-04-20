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

class JPAGDecoder {
 public:
  explicit JPAGDecoder(std::shared_ptr<pag::PAGDecoder> pagDecoder) : pagDecoder(pagDecoder) {
  }

  std::shared_ptr<pag::PAGDecoder> get() {
    std::lock_guard<std::mutex> autoLock(locker);
    return pagDecoder;
  }

  void clear() {
    std::lock_guard<std::mutex> autoLock(locker);
    pagDecoder = nullptr;
  }

 private:
  std::shared_ptr<pag::PAGDecoder> pagDecoder;
  std::mutex locker;
};
