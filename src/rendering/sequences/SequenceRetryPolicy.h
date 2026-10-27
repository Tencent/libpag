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

#include <algorithm>
#include <cstdint>

namespace pag {
// Single source of truth for the sequence decode retry backoff, shared by RenderCache (per-sequence
// cooldown) and PAGDecoder (per-decoder failed render cooldown) so the two layers stay aligned.
static constexpr int64_t SEQUENCE_RETRY_INITIAL_DELAY = 100000;
static constexpr int64_t SEQUENCE_RETRY_MAX_DELAY = 2000000;

/**
 * Returns the cooldown delay in microseconds for the given number of consecutive failures that have
 * already happened (0 for the first failure). The delay starts at SEQUENCE_RETRY_INITIAL_DELAY,
 * doubles on each additional failure, and is capped at SEQUENCE_RETRY_MAX_DELAY. A transient failure
 * that recovers on the next retry therefore only pauses briefly, while a persistent failure quickly
 * escalates to the ceiling to stop the renderer from re-rendering every frame on every loop.
 */
inline int64_t SequenceRetryDelay(uint32_t consecutiveFailures) {
  auto shift = std::min<uint32_t>(consecutiveFailures, 5);
  return std::min<int64_t>(SEQUENCE_RETRY_INITIAL_DELAY << shift, SEQUENCE_RETRY_MAX_DELAY);
}
}  // namespace pag
