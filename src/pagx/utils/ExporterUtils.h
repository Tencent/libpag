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

FillStrokeInfo CollectFillStroke(const std::vector<Element*>& contents);

Matrix BuildLayerMatrix(const Layer* layer);

Matrix BuildGroupMatrix(const Group* group);

/**
 * Converts text glyph runs into a list of glyph paths with transform matrices.
 * textPosX/textPosY are the base position for glyph placement.
 */
std::vector<GlyphPath> ComputeGlyphPaths(const Text& text, float textPosX, float textPosY);

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
 * Rasterizes a tgfx layer (including any masks) to a PNG-encoded Data object.
 * Returns nullptr if the layer has zero bounds or rasterization fails.
 */
std::shared_ptr<tgfx::Data> RenderMaskedLayer(GPUContext* gpu,
                                              const std::shared_ptr<tgfx::Layer>& root,
                                              const std::shared_ptr<tgfx::Layer>& targetLayer);

class ImagePattern;

/**
 * Rasterizes a tiled ImagePattern fill to a PNG-encoded Data object at the given pixel dimensions.
 * The image is drawn with the pattern's tile modes and matrix (offset relative to shapeBounds).
 * Returns nullptr if rendering fails.
 */
std::shared_ptr<tgfx::Data> RenderTiledPattern(GPUContext* gpu, const ImagePattern* pattern,
                                               int width, int height, float offsetX, float offsetY);

/**
 * Strips surrounding double-quote characters from a string.
 */
std::string StripQuotes(const std::string& s);

}  // namespace pagx
