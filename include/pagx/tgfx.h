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
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "tgfx/core/Image.h"
#include "tgfx/gpu/Recording.h"

namespace pagx {

std::shared_ptr<PAGImage> MakeFromTGFXImage(const std::shared_ptr<tgfx::Image>& image);
std::shared_ptr<tgfx::Image> GetTGFXImage(const std::shared_ptr<PAGImage>& image);

/**
 * Renders the scene into the surface using the given GPU context, flushes rendering commands,
 * and returns a Recording that can be submitted later. This allows tgfx power users to batch
 * multiple scene renderings and their own tgfx work under a single GPU submission.
 * The caller must hold the context lock (via Device::lockContext()). Does not submit or present.
 * @param scene the scene to render.
 * @param context the locked GPU context.
 * @param surface the surface to render into.
 * @param autoClear whether to clear the surface before rendering.
 * @return a Recording ready for submission, or nullptr on failure.
 */
std::unique_ptr<tgfx::Recording> Record(tgfx::Context* context,
                                        const std::shared_ptr<PAGScene>& scene,
                                        const std::shared_ptr<PAGSurface>& surface,
                                        bool autoClear = true);

}  // namespace pagx
