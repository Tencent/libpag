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
#include "pagx/types/Enums.h"
#include "pagx/types/Types.h"

// Base classes
#include "pagx/nodes/ColorSource.h"
#include "pagx/nodes/LayerFilter.h"
#include "pagx/nodes/LayerStyle.h"
#include "pagx/nodes/Node.h"
#include "pagx/nodes/VectorElement.h"

// Color sources
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/SolidColor.h"

// Vector elements - shapes
#include "pagx/nodes/Ellipse.h"
#include "pagx/nodes/Path.h"
#include "pagx/nodes/Polystar.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/nodes/TextSpan.h"

// Vector elements - painters
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Stroke.h"

// Vector elements - path modifiers
#include "pagx/nodes/MergePath.h"
#include "pagx/nodes/RoundCorner.h"
#include "pagx/nodes/TrimPath.h"

// Vector elements - text modifiers
#include "pagx/nodes/RangeSelector.h"
#include "pagx/nodes/TextLayout.h"
#include "pagx/nodes/TextModifier.h"
#include "pagx/nodes/TextPath.h"

// Vector elements - containers
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Repeater.h"

// Layer styles
#include "pagx/nodes/BackgroundBlurStyle.h"
#include "pagx/nodes/DropShadowStyle.h"
#include "pagx/nodes/InnerShadowStyle.h"

// Layer filters
#include "pagx/nodes/BlendFilter.h"
#include "pagx/nodes/BlurFilter.h"
#include "pagx/nodes/ColorMatrixFilter.h"
#include "pagx/nodes/DropShadowFilter.h"
#include "pagx/nodes/InnerShadowFilter.h"

// Resources
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/PathDataResource.h"

// Layer
#include "pagx/nodes/Layer.h"
