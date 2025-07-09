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

#include <cinttypes>

namespace pag {
/**
 * This structure stores encoded video sample data.
 */
struct VideoSample {
  /**
   * The byte data of the sample.
   */
  void* data = nullptr;
  /**
   * The size in bytes of the data.
   */
  size_t length = 0;

  /**
   * Sample's presentation time in microseconds.
   */
  int64_t time = 0;
};
}  // namespace pag
