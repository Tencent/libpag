/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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
#include <pag/file.h>
#include "utils/PAGExportSession.h"

namespace exporter {

void GetVideoSequence(std::shared_ptr<PAGExportSession> session,
                      std::vector<pag::Composition*>& compositions,
                      pag::VideoComposition* composition, float factor);

/**
 * Returns the offset in frames that should be applied to the compositionStartTime of the layer
 * referencing the given video composition, so that the exported sequence stays aligned with the
 * original timeline. The offset is converted from the sequence frame rate to the given frame rate
 * and returns 0 when the video composition starts at the first exported frame.
 */
pag::Frame GetVideoCompositionStartOffset(std::shared_ptr<PAGExportSession> session,
                                          pag::VideoComposition* composition, float frameRate);

}  // namespace exporter
