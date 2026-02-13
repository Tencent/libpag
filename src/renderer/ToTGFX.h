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
#include "pagx/nodes/PathData.h"
#include "pagx/types/BlendMode.h"
#include "pagx/types/Color.h"
#include "pagx/types/Data.h"
#include "pagx/types/FillRule.h"
#include "pagx/types/LayerPlacement.h"
#include "pagx/types/MaskType.h"
#include "pagx/types/Matrix.h"
#include "pagx/types/MergePathMode.h"
#include "pagx/types/Point.h"
#include "pagx/types/Rect.h"
#include "pagx/types/RepeaterOrder.h"
#include "pagx/types/SelectorTypes.h"
#include "pagx/types/StrokeStyle.h"
#include "pagx/types/TileMode.h"
#include "tgfx/core/BlendMode.h"
#include "tgfx/core/Color.h"
#include "tgfx/core/Data.h"
#include "tgfx/core/Matrix.h"
#include "tgfx/core/Path.h"
#include "tgfx/core/Rect.h"
#include "tgfx/core/Stroke.h"
#include "tgfx/core/TileMode.h"

// Forward declarations for tgfx types that conflict with pagx types when included directly.
namespace tgfx {
enum class FillRule;
enum class LayerPlacement;
enum class StrokeAlign;
enum class MergePathOp;
enum class LayerMaskType;
enum class RepeaterOrder;
enum class SelectorUnit;
enum class SelectorShape;
enum class SelectorMode;
}  // namespace tgfx

namespace pagx {

tgfx::Path PathDataToTGFXPath(const PathData& pathData);

tgfx::Point ToTGFX(const Point& point);

tgfx::Color ToTGFX(const Color& color);

tgfx::Matrix ToTGFX(const Matrix& matrix);

tgfx::Rect ToTGFX(const Rect& rect);

tgfx::Path ToTGFX(const PathData& pathData);

tgfx::LineCap ToTGFX(LineCap cap);

tgfx::LineJoin ToTGFX(LineJoin join);

tgfx::BlendMode ToTGFX(BlendMode mode);

tgfx::FillRule ToTGFX(FillRule rule);

tgfx::LayerPlacement ToTGFX(LayerPlacement placement);

tgfx::StrokeAlign ToTGFX(StrokeAlign align);

tgfx::MergePathOp ToTGFX(MergePathMode mode);

tgfx::LayerMaskType ToTGFXMaskType(MaskType type);

tgfx::RepeaterOrder ToTGFX(RepeaterOrder order);

tgfx::SelectorUnit ToTGFX(SelectorUnit unit);

tgfx::SelectorShape ToTGFX(SelectorShape shape);

tgfx::SelectorMode ToTGFX(SelectorMode mode);

tgfx::TileMode ToTGFX(TileMode mode);

std::shared_ptr<tgfx::Data> ToTGFXData(const std::shared_ptr<Data>& data);

}  // namespace pagx
