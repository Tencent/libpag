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
class Data;
}

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
  const PathData* pathData;
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

bool GetPNGDimensions(const uint8_t* data, size_t size, int* width, int* height);

bool GetPNGDimensionsFromPath(const std::string& path, int* width, int* height);

bool GetImagePNGDimensions(const Image* image, int* width, int* height);

bool IsJPEG(const uint8_t* data, size_t size);

std::shared_ptr<tgfx::Data> GetImageData(const Image* image);

bool HasNonASCII(const std::string& str);

std::string UTF8ToUTF16BEHex(const std::string& utf8);

}  // namespace pagx
