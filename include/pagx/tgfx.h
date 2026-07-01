/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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
#include "pagx/PAGImage.h"
#include "tgfx/core/Image.h"

namespace pagx {

/**
 * Creates a PAGImage that wraps an existing tgfx::Image, sharing ownership via shared_ptr. Use
 * this when the host already has a tgfx::Image (e.g. created via tgfx::Image::MakeFrom within a
 * held GL context) and wants to pass it to PAGXDocument::loadFileData without re-decoding.
 * @param image the tgfx::Image to wrap. Must not be nullptr.
 * @return a PAGImage sharing ownership of the given image.
 */
std::shared_ptr<PAGImage> MakeFromTGFXImage(const std::shared_ptr<tgfx::Image>& image);

/**
 * Returns the underlying tgfx::Image wrapped by a PAGImage, or nullptr if the PAGImage is null.
 * The returned shared_ptr shares ownership with the PAGImage.
 * @param image the PAGImage to unwrap.
 * @return the underlying tgfx::Image, or nullptr if image is null.
 */
std::shared_ptr<tgfx::Image> GetTGFXImage(const std::shared_ptr<PAGImage>& image);

}  // namespace pagx
