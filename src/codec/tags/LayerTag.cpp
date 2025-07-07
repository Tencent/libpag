/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "LayerTag.h"
#include "CameraOption.h"
#include "CompositionReference.h"
#include "EffectTag.h"
#include "ImageReference.h"
#include "LayerAttributes.h"
#include "LayerAttributesExtra.h"
#include "LayerStyleTag.h"
#include "MarkerTag.h"
#include "MaskTag.h"
#include "ShapeTag.h"
#include "SolidColor.h"
#include "Transform2D.h"
#include "Transform3D.h"
#include "codec/tags/CachePolicy.h"
#include "codec/tags/ImageFillRule.h"
#include "codec/tags/text/TextAnimatorTag.h"
#include "codec/tags/text/TextMoreOption.h"
#include "codec/tags/text/TextPathOption.h"
#include "codec/tags/text/TextSource.h"

namespace pag {

#define Condition(condition, expression) \
  if (condition) {                       \
    expression;                          \
  }

static void CheckLayerDuration(Layer* layer) {
  if (layer->duration <= 0) {
    // The duration cannot be zero, fix it when the value is parsed from an old file format.
    layer->duration = 1;
  }
}

void ReadTagsOfLayer(DecodeStream* stream, TagCode code, Layer* layer) {
  switch (code) {
    case TagCode::LayerAttributes:
      if (ReadTagBlock(stream, layer, LayerAttributesTag)) {
        CheckLayerDuration(layer);
      }
      break;
    case TagCode::LayerAttributesV2:
      if (ReadTagBlock(stream, layer, LayerAttributesTagV2)) {
        CheckLayerDuration(layer);
      }
      break;
    case TagCode::LayerAttributesV3:
      if (ReadTagBlock(stream, layer, LayerAttributesTagV3)) {
        CheckLayerDuration(layer);
      }
      break;
    case TagCode::LayerAttributesExtra:
      ReadTagBlock(stream, layer, LayerAttributesExtraTag);
      break;
    case TagCode::MaskBlock: {
      auto mask = ReadTagBlock(stream, MaskTag);
      if (mask) {
        layer->masks.push_back(mask);
      }
    } break;
    case TagCode::MaskBlockV2: {
      auto mask = ReadTagBlock(stream, MaskTagV2);
      if (mask) {
        layer->masks.push_back(mask);
      }
    } break;
    case TagCode::MarkerList: {
      ReadMarkerList(stream, &layer->markers);
    } break;
    case TagCode::Transform2D: {
      layer->transform = new Transform2D();
      auto transform = layer->transform;
      if (ReadTagBlock(stream, transform, Transform2DTag)) {

        auto hasPosition = (transform->position->animatable() ||
                            transform->position->getValueAt(0) != Point::Zero());
        auto hasXPosition =
            (transform->xPosition->animatable() || transform->xPosition->getValueAt(0) != 0);
        auto hasYPosition =
            (transform->yPosition->animatable() || transform->yPosition->getValueAt(0) != 0);
        if (hasPosition || (!hasXPosition && !hasYPosition)) {
          delete transform->xPosition;
          transform->xPosition = nullptr;
          delete transform->yPosition;
          transform->yPosition = nullptr;
        } else {
          delete transform->position;
          transform->position = nullptr;
        }
      }
    } break;
    case TagCode::Transform3D: {
      layer->transform3D = new Transform3D();
      auto transform = layer->transform3D;
      if (ReadTagBlock(stream, transform, Transform3DTag)) {
        auto hasPosition = (transform->position->animatable() ||
                            transform->position->getValueAt(0) != Point3D::Zero());
        auto hasXPosition =
            (transform->xPosition->animatable() || transform->xPosition->getValueAt(0) != 0);
        auto hasYPosition =
            (transform->yPosition->animatable() || transform->yPosition->getValueAt(0) != 0);
        auto hasZPosition =
            (transform->zPosition->animatable() || transform->zPosition->getValueAt(0) != 0);
        if (hasPosition || (!hasXPosition && !hasYPosition && !hasZPosition)) {
          delete transform->xPosition;
          transform->xPosition = nullptr;
          delete transform->yPosition;
          transform->yPosition = nullptr;
          delete transform->zPosition;
          transform->zPosition = nullptr;
        } else {
          delete transform->position;
          transform->position = nullptr;
        }
      }
    } break;
    case TagCode::CachePolicy:
      ReadCachePolicy(stream, layer);
      break;
    case TagCode::SolidColor:
      Condition(layer->type() == LayerType::Solid,
                ReadSolidColor(stream, static_cast<SolidLayer*>(layer)));
      break;
    case TagCode::TextSource:
      Condition(layer->type() == LayerType::Text,
                ReadTagBlock(stream, static_cast<TextLayer*>(layer), TextSourceTag));
      break;
    case TagCode::TextSourceV2:
      Condition(layer->type() == LayerType::Text,
                ReadTagBlock(stream, static_cast<TextLayer*>(layer), TextSourceTagV2));
      break;
    case TagCode::TextSourceV3:
      Condition(layer->type() == LayerType::Text,
                ReadTagBlock(stream, static_cast<TextLayer*>(layer), TextSourceTagV3));
      break;
    case TagCode::TextPathOption:
      if (layer->type() == LayerType::Text) {
        auto textLayer = static_cast<TextLayer*>(layer);
        textLayer->pathOption = new TextPathOptions();
        ReadTagBlock(stream, textLayer->pathOption, TextPathOptionTag);
      }
      break;
    case TagCode::TextMoreOption:
      if (layer->type() == LayerType::Text) {
        auto textLayer = static_cast<TextLayer*>(layer);
        textLayer->moreOption = new TextMoreOptions();
        ReadTagBlock(stream, textLayer->moreOption, TextMoreOptionTag);
      }
      break;
    case TagCode::TextAnimator:
      Condition(layer->type() == LayerType::Text,
                ReadTextAnimator(stream, static_cast<TextLayer*>(layer)));
      break;
    case TagCode::CompositionReference:
      Condition(layer->type() == LayerType::PreCompose,
                ReadCompositionReference(stream, static_cast<PreComposeLayer*>(layer)));
      break;
    case TagCode::ImageReference:
      Condition(layer->type() == LayerType::Image,
                ReadImageReference(stream, static_cast<ImageLayer*>(layer)));
      break;
    case TagCode::ImageFillRule:
    case TagCode::ImageFillRuleV2:
      Condition(layer->type() == LayerType::Image,
                ReadImageFillRule(stream, static_cast<ImageLayer*>(layer), code));
      break;
    case TagCode::CameraOption:
      if (layer->type() == LayerType::Camera) {
        auto cameraLayer = static_cast<CameraLayer*>(layer);
        cameraLayer->cameraOption = new CameraOption();
        ReadTagBlock(stream, cameraLayer->cameraOption, CameraOptionTag);
      }
      break;
    default:
      if (ReadEffect(stream, code, layer)) {
        break;
      }
      if (ReadLayerStyles(stream, code, layer)) {
        break;
      }
      if (layer->type() == LayerType::Shape &&
          ReadShape(stream, code, &(static_cast<ShapeLayer*>(layer)->contents))) {
        break;
      }
      break;
  }
}

Layer* ReadLayer(DecodeStream* stream) {
  auto layerType = static_cast<LayerType>(stream->readUint8());
  Layer* layer = nullptr;
  switch (layerType) {
    case LayerType::Null:
      layer = new NullLayer();
      break;
    case LayerType::Solid:
      layer = new SolidLayer();
      break;
    case LayerType::Text:
      layer = new TextLayer();
      break;
    case LayerType::Shape:
      layer = new ShapeLayer();
      break;
    case LayerType::Image:
      layer = new ImageLayer();
      break;
    case LayerType::PreCompose:
      layer = new PreComposeLayer();
      break;
    case LayerType::Camera:
      layer = new CameraLayer();
      break;
    default:
      layer = new Layer();
      break;
  }
  layer->id = stream->readEncodedUint32();
  ReadTags(stream, layer, ReadTagsOfLayer);
  return layer;
}

#undef Condition

static bool CheckMaskFeatherValid(Property<Point>* maskFeather) {
  if (maskFeather == nullptr) {
    return false;
  }
  if (maskFeather->animatable()) {
    return true;
  }
  return maskFeather->value.x != 0.0f || maskFeather->value.y != 0.0f;
}

TagCode WriteLayer(EncodeStream* stream, Layer* layer) {
  stream->writeUint8(static_cast<uint8_t>(layer->type()));
  stream->writeEncodedUint32(layer->id);

  if (layer->motionBlur) {
    WriteTagBlock(stream, layer, LayerAttributesTag);
    WriteTagBlock(stream, layer, LayerAttributesExtraTag);
  } else if (!layer->name.empty()) {
    WriteTagBlock(stream, layer, LayerAttributesTagV2);
  } else {
    WriteTagBlock(stream, layer, LayerAttributesTag);
  }

  for (auto& mask : layer->masks) {
    if (CheckMaskFeatherValid(mask->maskFeather)) {
      WriteTagBlock(stream, mask, MaskTagV2);
    } else {
      WriteTagBlock(stream, mask, MaskTag);
    }
  }
  if (layer->markers.size() > 0) {
    WriteTag(stream, &layer->markers, WriteMarkerList);
  }
  WriteEffects(stream, layer->effects);
  WriteLayerStyles(stream, layer->layerStyles);
  if (layer->transform3D != nullptr) {
    WriteTagBlock(stream, layer->transform3D, Transform3DTag);
  } else if (layer->transform != nullptr) {
    WriteTagBlock(stream, layer->transform, Transform2DTag);
  }
  if (layer->cachePolicy != CachePolicy::Auto) {
    WriteTag(stream, layer, WriteCachePolicy);
  }
  switch (layer->type()) {
    case LayerType::Solid:
      WriteTag(stream, static_cast<SolidLayer*>(layer), WriteSolidColor);
      break;
    case LayerType::Shape:
      WriteShape(stream, &(static_cast<ShapeLayer*>(layer)->contents));
      break;
    case LayerType::PreCompose:
      WriteTag(stream, static_cast<PreComposeLayer*>(layer), WriteCompositionReference);
      break;
    case LayerType::Image: {
      auto imageLayer = static_cast<ImageLayer*>(layer);
      WriteTag(stream, imageLayer, WriteImageReference);
      if (imageLayer->imageFillRule != nullptr &&
          (imageLayer->imageFillRule->scaleMode != PAGScaleMode::LetterBox ||
           imageLayer->imageFillRule->timeRemap != nullptr)) {
        WriteImageFillRule(stream, imageLayer->imageFillRule);
      }
    } break;
    case LayerType::Text: {
      auto textLayer = static_cast<TextLayer*>(layer);
      auto textDocument = textLayer->getTextDocument();
      if (textDocument != nullptr && textDocument->direction != TextDirection::Default) {
        WriteTagBlock(stream, textLayer, TextSourceTagV3);
      } else if (textDocument != nullptr && textDocument->backgroundAlpha != 0) {
        WriteTagBlock(stream, textLayer, TextSourceTagV2);
      } else {
        WriteTagBlock(stream, textLayer, TextSourceTag);
      }
      auto pathOption = textLayer->pathOption;
      auto moreOption = textLayer->moreOption;
      if (pathOption != nullptr && pathOption->path) {
        WriteTagBlock(stream, pathOption, TextPathOptionTag);
      }
      if (moreOption != nullptr) {
        WriteTagBlock(stream, moreOption, TextMoreOptionTag);
      }
      for (auto& animator : textLayer->animators) {
        WriteTag(stream, animator, WriteTextAnimator);
      }
    } break;
    case LayerType::Camera: {
      WriteTagBlock(stream, static_cast<CameraLayer*>(layer)->cameraOption, CameraOptionTag);
    }
    default:
      break;
  }
  WriteEndTag(stream);
  return TagCode::LayerBlock;
}

}  // namespace pag
