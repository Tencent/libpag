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
//  or required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" basis, without WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include "codec/utils/DecodeStream.h"
#include "codec/utils/EncodeStream.h"
#include "pagx/nodes/PathData.h"

namespace pagx {

// Path IO for the PAG v4 binary format (spec/pag_binary_spec.md §7). The legacy layout is
// kept (degenerate records + a packed point list) with two changes: records are 4 bits to
// add Quad, and a per-path step exponent adapts the quantization step to the node's
// maximum display scale.

/**
 * Returns the step exponent for a path quantized at the given maximum display scale
 * (spec §7.2): e = max(0, ceil(log2(scale))), clamped to 255. The step is 0.05 * 2^-e, so
 * scale <= 1 keeps the legacy 0.05 grid.
 */
uint8_t ComputeStepExponent(float maxDisplayScale);

/**
 * Writes a PathData with the given step exponent (spec §7.2): verb count varuint, step
 * exponent u8, 4-bit records, byte alignment, then the coordinate list packed with a
 * 5-bit width prefix. Glyph outlines pass 0 (font units keep the 0.05 step).
 */
void WritePAGPath(pag::EncodeStream* stream, const PathData* path, uint8_t stepExponent);

/**
 * Reads a path written by WritePAGPath into the target node. Returns false when the path
 * is malformed (unknown record); the caller decides whether the owning tag still makes
 * sense to keep.
 */
bool ReadPAGPath(pag::DecodeStream* stream, PathData* path);

}  // namespace pagx
