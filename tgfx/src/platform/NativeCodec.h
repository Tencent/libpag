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

#pragma once

#include "tgfx/core/ImageCodec.h"

namespace tgfx {
class NativeCodec {
 private:
  /**
   * If the file path represents an encoded image that current platform knows how to decode, returns
   * an ImageCodec that can decode it. Otherwise returns nullptr.
   */
  static std::shared_ptr<ImageCodec> MakeCodec(const std::string& filePath);

  /**
   * If the file bytes represents an encoded image that current platform knows how to decode,
   * returns an ImageCodec that can decode it. Otherwise returns nullptr.
   */
  static std::shared_ptr<ImageCodec> MakeCodec(std::shared_ptr<Data> imageBytes);

  friend class ImageCodec;
};
}  // namespace tgfx
