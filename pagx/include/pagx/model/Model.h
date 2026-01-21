/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

// Basic types and enums
#include "pagx/model/types/Enums.h"
#include "pagx/model/types/Types.h"

// Unified node type
#include "pagx/model/NodeType.h"

// Base classes
#include "pagx/model/ColorSource.h"
#include "pagx/model/Element.h"
#include "pagx/model/LayerFilter.h"
#include "pagx/model/LayerStyle.h"
#include "pagx/model/Resource.h"

// Color sources
#include "pagx/model/ColorStop.h"
#include "pagx/model/ConicGradient.h"
#include "pagx/model/DiamondGradient.h"
#include "pagx/model/ImagePattern.h"
#include "pagx/model/LinearGradient.h"
#include "pagx/model/RadialGradient.h"
#include "pagx/model/SolidColor.h"

// Vector elements - shapes
#include "pagx/model/Ellipse.h"
#include "pagx/model/Path.h"
#include "pagx/model/Polystar.h"
#include "pagx/model/Rectangle.h"
#include "pagx/model/TextSpan.h"

// Vector elements - painters
#include "pagx/model/Fill.h"
#include "pagx/model/Stroke.h"

// Vector elements - path modifiers
#include "pagx/model/MergePath.h"
#include "pagx/model/RoundCorner.h"
#include "pagx/model/TrimPath.h"

// Vector elements - text modifiers
#include "pagx/model/RangeSelector.h"
#include "pagx/model/TextLayout.h"
#include "pagx/model/TextModifier.h"
#include "pagx/model/TextPath.h"

// Vector elements - containers
#include "pagx/model/Group.h"
#include "pagx/model/Repeater.h"

// Layer styles
#include "pagx/model/BackgroundBlurStyle.h"
#include "pagx/model/DropShadowStyle.h"
#include "pagx/model/InnerShadowStyle.h"

// Layer filters
#include "pagx/model/BlendFilter.h"
#include "pagx/model/BlurFilter.h"
#include "pagx/model/ColorMatrixFilter.h"
#include "pagx/model/DropShadowFilter.h"
#include "pagx/model/InnerShadowFilter.h"

// Resources
#include "pagx/model/Composition.h"
#include "pagx/model/Image.h"
#include "pagx/model/PathDataResource.h"

// Layer
#include "pagx/model/Layer.h"
