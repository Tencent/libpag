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

#include "LayerStyleTag.h"
#include "layerStyles/DropShadowStyle.h"
#include "layerStyles/DropShadowStyleV2.h"
#include "layerStyles/GradientOverlayStyle.h"

namespace pag {
bool ReadLayerStyles(DecodeStream* stream, TagCode code, Layer* layer) {
  LayerStyle* style = nullptr;
  switch (code) {
    case TagCode::DropShadowStyle: {
      auto dropShadowStyle = ReadTagBlock(stream, DropShadowStyleTag);
      dropShadowStyle->spread = new Property<Percent>();
      dropShadowStyle->spread->value = 0.0f;  // set default value
      style = dropShadowStyle;
    } break;
    case TagCode::DropShadowStyleV2:
      style = ReadTagBlock(stream, DropShadowStyleTagV2);
      break;
    case TagCode::GradientOverlayStyle:
      style = ReadTagBlock(stream, GradientOverlayStyleTag);
      break;
    default:
      break;
  }
  if (style) {
    layer->layerStyles.push_back(style);
  }
  return style != nullptr;
}

void WriteLayerStyles(EncodeStream* stream, const std::vector<LayerStyle*>& layerStyles) {
  for (auto& style : layerStyles) {
    switch (style->type()) {
      case LayerStyleType::DropShadow: {
        auto spread = static_cast<DropShadowStyle*>(style)->spread;
        auto size = static_cast<DropShadowStyle*>(style)->size;
        if ((size != nullptr && size->animatable()) ||
            (spread != nullptr &&
             (spread->animatable() || spread->value != 0.0f))) {  // if not default value
          WriteTagBlock(stream, static_cast<DropShadowStyle*>(style), DropShadowStyleTagV2);
        } else {
          WriteTagBlock(stream, static_cast<DropShadowStyle*>(style), DropShadowStyleTag);
        }
        break;
      }
      case LayerStyleType::GradientOverlay:
        WriteTagBlock(stream, static_cast<GradientOverlayStyle*>(style), GradientOverlayStyleTag);
        break;
      default:
        break;
    }
  }
}
}  // namespace pag
