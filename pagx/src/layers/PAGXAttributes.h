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

#include <memory>
#include <string>
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/Point.h"
#include "tgfx/core/Stroke.h"
#include "tgfx/core/TileMode.h"
#include "tgfx/layers/LayerMaskType.h"
#include "tgfx/layers/StrokeAlign.h"
#include "tgfx/layers/vectors/FillStyle.h"
#include "tgfx/layers/vectors/MergePath.h"
#include "tgfx/layers/vectors/Polystar.h"
#include "tgfx/layers/vectors/TrimPath.h"
#include "tgfx/svg/xml/XMLDOM.h"

namespace pagx {

using namespace tgfx;

class PAGXAttributes {
 public:
  static float ParseFloat(const std::shared_ptr<DOMNode>& node, const std::string& name,
                          float defaultValue = 0.0f);

  static bool ParseBool(const std::shared_ptr<DOMNode>& node, const std::string& name,
                        bool defaultValue = true);

  static std::string ParseString(const std::shared_ptr<DOMNode>& node, const std::string& name,
                                 const std::string& defaultValue = "");

  static Color ParseColor(const std::string& value);

  static Point ParsePoint(const std::string& value, Point defaultValue = Point::Zero());

  static Matrix ParseMatrix(const std::string& value);

  static BlendMode ParseBlendMode(const std::string& value);

  static PathFillType ParseFillRule(const std::string& value);

  static LineCap ParseLineCap(const std::string& value);

  static LineJoin ParseLineJoin(const std::string& value);

  static TileMode ParseTileMode(const std::string& value);

  static LayerMaskType ParseMaskType(const std::string& value);

  static PolystarType ParsePolystarType(const std::string& value);

  static TrimPathType ParseTrimPathType(const std::string& value);

  static MergePathOp ParseMergePathOp(const std::string& value);

  static StrokeAlign ParseStrokeAlign(const std::string& value);

  static std::vector<float> ParseDashes(const std::string& value);

  static std::string GetTextContent(const std::shared_ptr<DOMNode>& node);
};

}  // namespace pagx
