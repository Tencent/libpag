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
 * Textures and FrameBuffers can be stored such that (0, 0) in texture space may correspond to
 * either the top-left or bottom-left content pixel.
 */
enum class ImageOrigin {
  /**
   * The default origin for all off-screen rendering. Use ImageOrigin::TopLeft origin for textures
   * which are from HardwareBuffers (Android), CVPixelBuffers (iOS), or normally created by backend
   * APIs. Note: ImageOrigin::TopLeft is actual bottom-left origin for OpenGL backend.
   */
  TopLeft,
  /**
   * Use this origin to flip the content on y-axis if the GPU backend has different origin to your
   * system views. It is usually used when on-screen rendering.
   */
  BottomLeft
};
}  // namespace tgfx
