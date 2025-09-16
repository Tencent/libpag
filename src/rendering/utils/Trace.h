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

#include "tgfx/core/Pixmap.h"
#include "tgfx/core/Surface.h"

namespace pag {
/**
 * Trace() provides a utility to view the texture.
 */
void Trace(std::shared_ptr<tgfx::Surface> surface, const std::string& path = "");

/**
 * Trace() provides a utility to view the Bitmap.
 */
void Trace(const tgfx::Bitmap& bitmap, const std::string& tag = "");

/**
 * Trace() provides a utility to view the Bitmap.
 */
void Trace(const tgfx::Pixmap& pixmap, const std::string& tag = "");
}  // namespace pag
