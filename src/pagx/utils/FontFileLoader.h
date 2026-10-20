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
#include <memory>
#include <string>
#include <vector>

namespace tgfx {
class Typeface;
}

namespace pagx {

class FontConfig;

/**
 * Creates a typeface from a font file on disk. TrueType/OpenType fonts and collections are opened
 * by path, while a WOFF2 file is decoded to its SFNT form in memory first, since the font backends
 * only read SFNT containers. Returns nullptr when the file cannot be read or has no face at
 * ttcIndex.
 */
std::shared_ptr<tgfx::Typeface> MakeTypefaceFromFile(const std::string& path, int ttcIndex = 0);

/**
 * Creates a typeface from in-memory font bytes, decoding WOFF2 data to SFNT first. Returns nullptr
 * when the bytes hold no usable face at ttcIndex.
 */
std::shared_ptr<tgfx::Typeface> MakeTypefaceFromBytes(const void* bytes, size_t length,
                                                      int ttcIndex = 0);

/**
 * Returns the number of faces SFNT font bytes hold: the count a TrueType collection records in its
 * `ttcf` header (bounded, since a malformed header may claim any number), or 1 for any other font.
 * Returns 0 when the bytes are too short to tell. WOFF2 data has to be decoded first. The font
 * backend cannot be asked instead: the CoreGraphics backend ignores the face index and returns the
 * first face for every one of them.
 */
int CountFontFaces(const void* bytes, size_t length);

/**
 * Loads every face of a font file in face-index order: a single entry for a plain font, one per
 * face for a collection. A WOFF2 file is decoded once and shared by all of its faces. Returns an
 * empty list when the file cannot be read.
 */
std::vector<std::shared_ptr<tgfx::Typeface>> MakeTypefacesFromFile(const std::string& path);

/**
 * Loads every face of in-memory font bytes the way MakeTypefacesFromFile loads a file. The bytes
 * are copied (or decoded, for WOFF2) once and shared by all faces. Returns an empty list when the
 * bytes hold no usable font.
 */
std::vector<std::shared_ptr<tgfx::Typeface>> MakeTypefacesFromBytes(const void* bytes,
                                                                    size_t length);

/**
 * Registers every face of a font file in fontConfig under the (family, style) its name table
 * carries, or under familyOverride when that is non-empty. Registering only face 0 would cover a
 * single-face file but merely the first face of a collection: PingFang.ttc holds 24 faces, and a
 * request for "PingFang SC Medium" resolves to face 7, which is unreachable unless every face has
 * its own key. When addToFallbackChain is true each face is also appended to the fallback chain,
 * which keeps unicode-range subset files sharing one (family, style) key reachable even though
 * the primary registration of that key holds only one of them. Returns the number of faces
 * registered, 0 when the file cannot be read.
 */
int RegisterFontFileFaces(FontConfig* fontConfig, const std::string& path, bool addToFallbackChain,
                          const std::string& familyOverride = "");

/**
 * Registers every face of an in-memory font file the same way RegisterFontFileFaces registers a
 * file on disk, each face under the (family, style) its name table carries, or under
 * familyOverride when that is non-empty. WOFF2 data is decoded once and the decoded SFNT is what
 * gets registered, so the registrations never decode it again. Returns the number of faces
 * registered, 0 when the bytes hold no usable font.
 */
int RegisterFontDataFaces(FontConfig* fontConfig, const void* bytes, size_t length,
                          bool addToFallbackChain, const std::string& familyOverride = "");

}  // namespace pagx
