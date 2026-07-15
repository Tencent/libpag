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
 * Inspects the magic bytes of an in-memory image and returns the canonical MIME type
 * (`"image/png"`, `"image/jpeg"`, `"image/webp"`, `"image/gif"`). Returns `nullptr`
 * when the buffer is too short or the magic bytes do not match a recognised format.
 *
 * The detector is intentionally narrow: it sniffs only the four formats that the
 * libpag image pipeline produces or consumes at the boundary (PNG/JPEG/WebP/GIF).
 * Anything else (TIFF, BMP, AVIF, HEIC, …) is reported as unknown so the caller can
 * decide how to handle it instead of being silently mislabelled.
 */
const char* DetectImageMime(const uint8_t* bytes, size_t size);

/**
 * Like `DetectImageMime`, but falls back to `"image/png"` instead of `nullptr` when
 * the magic bytes do not match a recognised format. Use this from serialisers that
 * MUST emit a non-empty MIME label in their output (e.g. `data:<mime>;base64,...`
 * URIs in PAGX/SVG documents). Decoders downstream of those serialisers sniff the
 * payload bytes themselves, so an unknown buffer reaching this fallback still
 * round-trips correctly through PAGX and tgfx — the label is purely for the
 * benefit of HTML preview / `<img src="data:…">` consumers.
 */
const char* DetectImageMimeOrPNG(const uint8_t* bytes, size_t size);

}  // namespace pagx
