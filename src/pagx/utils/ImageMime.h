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

#include <cstddef>
#include <cstdint>

namespace pagx {

/**
 * Inspects the magic bytes of an in-memory image and returns the canonical MIME type, or
 * `nullptr` when the buffer is too short or the magic bytes match no known signature.
 *
 * The four formats `<Image>` may carry per the PAGX spec — PNG, JPEG, WebP and GIF — are what
 * the libpag image pipeline produces and consumes at the boundary. Recognised beyond that set
 * are the formats that reach a document from the outside (AVIF and HEIC via `<img>` /
 * `background-image`, SVG via an inlined icon data URI) so a caller can report the offending
 * format by name instead of mislabelling the payload.
 */
const char* DetectImageMime(const uint8_t* bytes, size_t size);

/**
 * Returns true when `mime` is one of the formats `<Image>` / `<Glyph image>` may carry.
 *
 * The supported set is pinned by the PAGX spec ("支持格式：PNG、JPEG、WebP、GIF"): it is
 * exactly the set every renderer is required to decode, so bytes outside it — including a
 * `nullptr`/unknown `mime` — cannot be assumed to render anywhere. Callers that are about to
 * persist image bytes into a document must transcode first (or report the offending format);
 * this is the single source of truth for that decision.
 */
bool IsSupportedImageMime(const char* mime);

}  // namespace pagx
