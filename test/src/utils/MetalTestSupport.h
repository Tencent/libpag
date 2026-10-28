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

namespace pag {
/**
 * Clears the shared Metal glyph atlas before independent pixel comparisons. Call after previous
 * CPU rendering tasks finish and before drawing, without holding a device lock. The lock is
 * released before returning. Returns false if the default Metal device cannot be created or locked.
 * This test-only helper is compiled only for the Metal backend.
 */
bool ResetMetalAtlasForComparison();
}  // namespace pag
