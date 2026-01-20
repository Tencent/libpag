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

#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include "tgfx/core/Color.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/svg/SVGDOM.h"
#include "tgfx/svg/node/SVGCircle.h"
#include "tgfx/svg/node/SVGContainer.h"
#include "tgfx/svg/node/SVGEllipse.h"
#include "tgfx/svg/node/SVGGradient.h"
#include "tgfx/svg/node/SVGLine.h"
#include "tgfx/svg/node/SVGMask.h"
#include "tgfx/svg/node/SVGNode.h"
#include "tgfx/svg/node/SVGPath.h"
#include "tgfx/svg/node/SVGPoly.h"
#include "tgfx/svg/node/SVGRect.h"
#include "tgfx/svg/node/SVGRoot.h"
#include "tgfx/svg/node/SVGText.h"

namespace pagx {

using namespace tgfx;

struct PatternInfo {
  std::string patternId = {};
  std::string imageId = {};
  std::string imageData = {};
  float imageWidth = 0.0f;
  float imageHeight = 0.0f;
  float patternWidth = 0.0f;   // in objectBoundingBox units (0-1)
  float patternHeight = 0.0f;  // in objectBoundingBox units (0-1)
  bool isObjectBoundingBox = true;
};

struct ColorSourceUsage {
  int count = 0;
  std::string type = {};  // "gradient" or "pattern"
};

struct MaskInfo {
  std::string maskId = {};
  const SVGNode* maskNode = nullptr;
  bool isLuminance = false;
};

class SVGToPAGXConverter {
 public:
  explicit SVGToPAGXConverter(const std::shared_ptr<SVGDOM>& svgDOM);

  std::string convert();

 private:
  void writeHeader();
  void writeResources();
  void writeLayers();

  void convertNode(const SVGNode* node, int depth, bool needScopeIsolation);
  void convertContainer(const SVGContainer* container, int depth);
  void convertRect(const SVGRect* rect, int depth);
  void convertCircle(const SVGCircle* circle, int depth);
  void convertEllipse(const SVGEllipse* ellipse, int depth);
  void convertPath(const SVGPath* path, int depth);
  void convertLine(const SVGLine* line, int depth);
  void convertPoly(const SVGPoly* poly, int depth);
  void convertText(const SVGText* text, int depth);

  void writeFillStyle(const SVGNode* node, int depth, float shapeX = 0, float shapeY = 0,
                      float shapeWidth = 0, float shapeHeight = 0);
  void writeStrokeStyle(const SVGNode* node, int depth, float shapeX = 0, float shapeY = 0,
                        float shapeWidth = 0, float shapeHeight = 0);

  void writeInlineGradient(const std::string& gradientId, int depth);
  void writeInlineImagePattern(const PatternInfo& patternInfo, int depth, float shapeX,
                               float shapeY, float shapeWidth, float shapeHeight);

  std::string colorToHex(const Color& color) const;
  std::string colorToString(const SVGPaint& paint) const;
  std::string matrixToString(const Matrix& matrix) const;
  std::string lengthToFloat(const SVGLength& length, float containerSize) const;

  void indent(int depth);
  void writeAttribute(const std::string& name, const std::string& value);
  void writeAttribute(const std::string& name, float value);

  void collectGradients();
  void collectPatterns();
  void collectMasks();
  void collectUsedMasks(const SVGNode* node);
  void countColorSourceUsages();
  void countColorSourceUsagesFromNode(const SVGNode* node);
  int countRenderableChildren(const SVGContainer* container) const;

  bool hasOnlyFill(const SVGNode* node) const;
  bool hasOnlyStroke(const SVGNode* node) const;
  bool areRectsEqual(const SVGRect* a, const SVGRect* b) const;
  void convertChildren(const std::vector<std::shared_ptr<SVGNode>>& children, int depth);
  void convertRectWithStroke(const SVGRect* fillRect, const SVGRect* strokeRect, int depth);

  std::string getMaskId(const SVGNode* node) const;
  void writeMaskLayers(int depth);

  std::shared_ptr<SVGDOM> _svgDOM = nullptr;
  std::ostringstream _output = {};
  float _width = 0.0f;
  float _height = 0.0f;
  std::map<std::string, const SVGNode*> _gradients = {};
  std::map<std::string, PatternInfo> _patterns = {};
  std::map<std::string, std::string> _images = {};
  std::map<std::string, ColorSourceUsage> _colorSourceUsages = {};
  std::map<std::string, MaskInfo> _masks = {};
  std::set<std::string> _usedMasks = {};
};

}  // namespace pagx
