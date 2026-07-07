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
#include "tgfx/core/Surface.h"
#include "tgfx/gpu/Recording.h"

namespace pagx {

/**
 * Creates a PAGImage wrapping an existing tgfx::Image, sharing ownership. Use this when the host
 * already has a tgfx::Image and wants to pass it to PAGXDocument::loadFileData without re-decoding
 * or re-locking the GPU context.
 * @param image the tgfx::Image to wrap.
 * @return a PAGImage sharing ownership, or nullptr if image is null.
 */
std::shared_ptr<PAGImage> MakeFrom(const std::shared_ptr<tgfx::Image>& image);

/**
 * Renders a scene into a surface using a locked GPU context and returns a Recording that can be
 * submitted later. This allows batching multiple scene renders and custom tgfx work under a single
 * GPU submission. Does not submit or present — the caller controls both.
 * @param context the locked GPU context.
 * @param scene the scene to render.
 * @param surface the surface to render into.
 * @param autoClear whether to clear the surface before rendering.
 * @return a Recording ready for submission, or nullptr on failure.
 */
std::unique_ptr<tgfx::Recording> Record(tgfx::Context* context,
                                        const std::shared_ptr<PAGScene>& scene,
                                        const std::shared_ptr<PAGSurface>& surface,
                                        bool autoClear = true);

/**
 * Creates a PAGSurface wrapping an existing tgfx::Surface, sharing ownership. Use this when the
 * host already has a tgfx::Surface and wants to render a PAGScene into it via Record() without
 * creating a separate render target.
 * @param surface the tgfx::Surface to wrap.
 * @return a PAGSurface sharing ownership, or nullptr if surface is null.
 */
std::shared_ptr<PAGSurface> MakeFrom(const std::shared_ptr<tgfx::Surface>& surface);

}  // namespace pagx
