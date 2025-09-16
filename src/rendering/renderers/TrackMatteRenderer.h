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

#include "FilterRenderer.h"
#include "pag/pag.h"
#include "rendering/graphics/Recorder.h"

namespace pag {
struct TrackMatte {
  std::shared_ptr<Modifier> modifier = nullptr;
  std::shared_ptr<Graphic> colorGlyphs = nullptr;
};

class TrackMatteRenderer {
 public:
  /**
   * Returns nullptr if trackMatteLayer is nullptr.
   */
  static std::unique_ptr<TrackMatte> Make(PAGLayer* trackMatteOwner);

  /**
   * Returns nullptr if trackMatteLayer is nullptr.
   */
  static std::unique_ptr<TrackMatte> Make(Layer* trackMatteOwner, Frame layerFrame);
};
}  // namespace pag
