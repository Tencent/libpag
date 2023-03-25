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

namespace tgfx {
/**
 * WebImageCodec provides convenience functions to enable/disable the async support for decoding web
 * images.
 */
class WebCodec {
 public:
  /**
   * Returns true if the async support for decoding web images is enabled. The default value is
   * false.
   */
  static bool AsyncSupport();

  /**
   * Enables or disables the async support for decoding web images. If set to true, the ImageBuffers
   * generated from the web platform will not be fully decoded buffers. Instead, they will trigger
   * promise-awaiting calls before generating textures, which can speed up the process of decoding
   * multiple images simultaneously. Do not set it to true if your rendering process may require
   * multiple flush() calls to the screen Surface during one single frame. Otherwise, it may result
   * in screen tearing.
   */
  static void SetAsyncSupport(bool enabled);
};
}  // namespace tgfx
