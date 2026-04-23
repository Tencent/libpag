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

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "pagx/TextLayoutParams.h"
#include "pagx/nodes/Element.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Stroke.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/types/Matrix.h"

namespace tgfx {
class Context;
class Data;
class Device;
class Layer;
}  // namespace tgfx

namespace pagx {

class Image;
class PathData;
class Text;

struct FillStrokeInfo {
  const Fill* fill = nullptr;
  const Stroke* stroke = nullptr;
  const TextBox* textBox = nullptr;
};

struct GlyphPath {
  Matrix transform;
  const PathData* pathData = nullptr;
};

struct GlyphImage {
  // Maps the image's pixel-coord rect (0,0,imageWidth,imageHeight) to
  // layer-space coords. The glyph's design-space `offset` is already baked into
  // this matrix so callers don't need to know about it.
  Matrix transform;
  const Image* image = nullptr;
};

FillStrokeInfo CollectFillStroke(const std::vector<Element*>& contents);

Matrix BuildLayerMatrix(const Layer* layer);

Matrix BuildGroupMatrix(const Group* group);

/**
 * Converts text glyph runs into a list of vector glyph paths with transform matrices.
 * Image-only glyphs are skipped — use ComputeGlyphImages for those.
 * textPosX/textPosY are the base position for glyph placement.
 */
std::vector<GlyphPath> ComputeGlyphPaths(const Text& text, float textPosX, float textPosY);

/**
 * Converts text glyph runs into a list of bitmap glyphs with transform matrices.
 * Vector glyphs (without an `image`) are skipped — use ComputeGlyphPaths for those.
 * textPosX/textPosY are the base position for glyph placement (same conventions
 * as ComputeGlyphPaths so vector and bitmap glyphs in the same run line up).
 */
std::vector<GlyphImage> ComputeGlyphImages(const Text& text, float textPosX, float textPosY);

/**
 * Computes both vector glyph paths and bitmap glyph images in a single traversal
 * of `text`. Equivalent to calling ComputeGlyphPaths and ComputeGlyphImages in
 * sequence, but walks the glyph runs once — use this when a caller needs both
 * kinds from the same text element.
 */
void ComputeGlyphPathsAndImages(const Text& text, float textPosX, float textPosY,
                                std::vector<GlyphPath>* paths, std::vector<GlyphImage>* images);

FillRule DetectMaskFillRule(const Layer* maskLayer);

/**
 * Decomposes the X and Y scale factors of a 2D affine matrix.  The X scale is the
 * length of the (a, b) basis vector; the Y scale is derived from |det(m)| / sx so
 * that sx * sy equals the absolute area scale of the matrix (and sy stays positive
 * even when the matrix is mirrored).  Returns sx = sy = 0 for degenerate matrices.
 */
void DecomposeScale(const Matrix& m, float* sx, float* sy);

bool GetPNGDimensions(const uint8_t* data, size_t size, int* width, int* height);

bool GetPNGDimensionsFromPath(const std::string& path, int* width, int* height);

bool GetImagePNGDimensions(const Image* image, int* width, int* height);

bool GetJPEGDimensions(const uint8_t* data, size_t size, int* width, int* height);

bool GetWebPDimensions(const uint8_t* data, size_t size, int* width, int* height);

bool GetImageDimensions(const Image* image, int* width, int* height);

/**
 * Reads the physical DPI from a raw PNG byte stream. Returns false when the
 * stream is not PNG, the pHYs chunk is absent, or the unit is not meters.
 */
bool GetPNGDPI(const uint8_t* data, size_t size, float* dpiX, float* dpiY);

/**
 * Reads the physical DPI from a raw JPEG byte stream via the JFIF APP0
 * segment. Returns false when the stream is not JPEG or no JFIF density is
 * declared.
 */
bool GetJPEGDPI(const uint8_t* data, size_t size, float* dpiX, float* dpiY);

bool GetImageDPI(const Image* image, float* dpiX, float* dpiY);

bool IsJPEG(const uint8_t* data, size_t size);

bool IsWebP(const uint8_t* data, size_t size);

std::shared_ptr<tgfx::Data> ConvertWebPToPNG(const std::shared_ptr<tgfx::Data>& webpData);

std::shared_ptr<tgfx::Data> GetImageData(const Image* image);

bool HasNonASCII(const std::string& str);

std::string UTF8ToUTF16BEHex(const std::string& utf8);

/**
 * Manages a lazily-created GPU device and context for off-screen rendering.
 * Reuse a single instance across multiple render calls to avoid repeated GL context creation.
 */
class GPUContext {
 public:
  ~GPUContext();

  GPUContext(const GPUContext&) = delete;
  GPUContext& operator=(const GPUContext&) = delete;
  GPUContext() = default;

  tgfx::Context* lockContext();
  void unlock();

 private:
  std::shared_ptr<tgfx::Device> _device = nullptr;
};

/**
 * Rasterizes a tgfx layer (including any masks) to a PNG-encoded Data object. `pixelScale` is the
 * ratio of the desired output pixel density to the layer's logical coordinate space (e.g. 2.0 for
 * 2x density). The encoded PNG has dimensions ceil(logicalSize * pixelScale); callers keep using
 * the logical bounds when placing the PNG so that consumers scale it back up on display.
 * Returns nullptr if the layer has zero bounds or rasterization fails.
 */
std::shared_ptr<tgfx::Data> RenderMaskedLayer(GPUContext* gpu,
                                              const std::shared_ptr<tgfx::Layer>& root,
                                              const std::shared_ptr<tgfx::Layer>& targetLayer,
                                              float pixelScale);

/**
 * Rasterizes the full scene rooted at `root`, clipped to the global bounds of `targetLayer`, to
 * a PNG-encoded Data object. Unlike RenderMaskedLayer (which draws only the target against an
 * empty canvas), this renders every layer from `root` downward so that backdrop pixels
 * participate in the composite. Use when the target layer's visual result depends on pixels
 * drawn below it — e.g. a non-Normal `Layer.blendMode` that must blend against the backdrop.
 * `pixelScale` controls the output pixel density (see RenderMaskedLayer).
 * Returns nullptr if the target has zero bounds or rasterization fails.
 */
std::shared_ptr<tgfx::Data> RenderLayerCompositeWithBackdrop(
    GPUContext* gpu, const std::shared_ptr<tgfx::Layer>& root,
    const std::shared_ptr<tgfx::Layer>& targetLayer, float pixelScale);

class ImagePattern;

/**
 * Rasterizes a tiled ImagePattern fill to a PNG-encoded Data object. `width`/`height` describe
 * the logical tile size and `offsetX`/`offsetY` the pattern matrix offset in the same logical
 * space. `pixelScale` controls the output pixel density: the encoded PNG has dimensions
 * ceil(width * pixelScale) × ceil(height * pixelScale). Returns nullptr if rendering fails.
 */
std::shared_ptr<tgfx::Data> RenderTiledPattern(GPUContext* gpu, const ImagePattern* pattern,
                                               int width, int height, float offsetX, float offsetY,
                                               float pixelScale);

/**
 * Returns the effective width/height of a TextBox for typesetting and shape sizing.
 * Falls back to the layout-resolved dimensions when no explicit width/height was supplied
 * (e.g. when the TextBox is positioned via left/right/top/bottom constraints), and returns
 * NaN when neither an explicit nor a resolved value is available (auto-sized in that axis).
 */
float EffectiveTextBoxWidth(const TextBox* box);
float EffectiveTextBoxHeight(const TextBox* box);

/**
 * Builds TextLayoutParams from a TextBox's attributes with padding-adjusted content dimensions.
 */
TextLayoutParams MakeTextBoxParams(const TextBox* box);

/**
 * Builds TextLayoutParams from a standalone Text element's attributes.
 */
TextLayoutParams MakeStandaloneParams(const Text* text);

/**
 * Strips surrounding double-quote characters from a string.
 */
std::string StripQuotes(const std::string& s);

}  // namespace pagx
