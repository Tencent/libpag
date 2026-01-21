/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "pagx/nodes/Resource.h"
#include "pagx/nodes/ConicGradient.h"
#include "pagx/nodes/Composition.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/LinearGradient.h"
#include "pagx/nodes/PathDataResource.h"
#include "pagx/nodes/RadialGradient.h"
#include "pagx/nodes/SolidColor.h"

namespace pagx {

const char* ResourceTypeName(ResourceType type) {
  switch (type) {
    case ResourceType::Image:
      return "Image";
    case ResourceType::PathData:
      return "PathData";
    case ResourceType::ColorSource:
      return "ColorSource";
    case ResourceType::Composition:
      return "Composition";
    default:
      return "Unknown";
  }
}

static const std::string emptyString = "";

const std::string& ImageResource::id() const {
  if (image) {
    return image->id;
  }
  return emptyString;
}

const std::string& PathDataResourceWrapper::id() const {
  if (pathData) {
    return pathData->id;
  }
  return emptyString;
}

const std::string& ColorSourceResource::id() const {
  if (!colorSource) {
    return emptyString;
  }
  switch (colorSource->type()) {
    case ColorSourceType::SolidColor:
      return static_cast<SolidColor*>(colorSource.get())->id;
    case ColorSourceType::LinearGradient:
      return static_cast<LinearGradient*>(colorSource.get())->id;
    case ColorSourceType::RadialGradient:
      return static_cast<RadialGradient*>(colorSource.get())->id;
    case ColorSourceType::ConicGradient:
      return static_cast<ConicGradient*>(colorSource.get())->id;
    case ColorSourceType::DiamondGradient:
      return static_cast<DiamondGradient*>(colorSource.get())->id;
    case ColorSourceType::ImagePattern:
      return static_cast<ImagePattern*>(colorSource.get())->id;
    default:
      return emptyString;
  }
}

const std::string& CompositionResource::id() const {
  if (composition) {
    return composition->id;
  }
  return emptyString;
}

}  // namespace pagx
