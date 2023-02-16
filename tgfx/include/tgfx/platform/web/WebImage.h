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
 * WebImage provides convenience functions to enable/disable the async support for decoding web
 * images.
 */
class WebImage {
 public:
  /**
   * Enables or disables the the async support for decoding web images. The default value is false.
   * If set to true, the ImageCodec.makeBuffer() method will not trigger a promise-awaiting call and
   * will return an ImageBuffer that contains a Promise instead of a fully decoded HTMLImageElement,
   * which can speed up the process of decoding multiple images at the same time. Do not set it to
   * true if your rendering process may trigger multiple flush() calls of the screen Surface during
   * one single frame. Otherwise, it may result in screen tearing.
   */
  static void SetAsyncSupport(bool enabled);
};
}  // namespace tgfx
