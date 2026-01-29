/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "pag/c/pag_types.h"
#include "pag/pag.h"
#include "rendering/PAGAnimator.h"
#include "tgfx/gpu/Device.h"

enum class PAGObjectType : uint8_t {
  Unknown = 0,
  ByteData,
  TextDocument,
  Layer,
  Player,
  Surface,
  Image,
  Font,
  BackendTexture,
  BackendSemaphore,
  Decoder,
  PAGAnimator,

  Count,
};

struct pag_byte_data {
  explicit pag_byte_data(std::unique_ptr<pag::ByteData> byteData) : p(std::move(byteData)) {
  }

  PAGObjectType type = PAGObjectType::ByteData;
  std::unique_ptr<pag::ByteData> p;
};

struct pag_text_document {
  explicit pag_text_document(std::shared_ptr<pag::TextDocument> textDocument)
      : p(std::move(textDocument)) {
  }

  PAGObjectType type = PAGObjectType::TextDocument;
  std::shared_ptr<pag::TextDocument> p;
};

struct pag_layer {
  explicit pag_layer(std::shared_ptr<pag::PAGLayer> layer) : p(std::move(layer)) {
  }

  PAGObjectType type = PAGObjectType::Layer;
  std::shared_ptr<pag::PAGLayer> p;
};

struct pag_player {
  explicit pag_player(std::shared_ptr<pag::PAGPlayer> player) : p(std::move(player)) {
  }

  PAGObjectType type = PAGObjectType::Player;
  std::shared_ptr<pag::PAGPlayer> p;
};

namespace pag {
class PAGSurfaceExt {
 public:
  explicit PAGSurfaceExt(std::shared_ptr<PAGSurface> surface) : surface(std::move(surface)) {
  }

  BackendTexture getFrontTexture();

  BackendTexture getBackTexture();

  HardwareBufferRef getFrontHardwareBuffer();

  HardwareBufferRef getBackHardwareBuffer();

  std::shared_ptr<PAGSurface> surface;
};
}  // namespace pag

struct pag_surface {
  explicit pag_surface(std::shared_ptr<pag::PAGSurface> surface)
      : p(surface), ext(std::make_shared<pag::PAGSurfaceExt>(surface)) {
  }

  PAGObjectType type = PAGObjectType::Surface;
  std::shared_ptr<pag::PAGSurface> p;
  std::shared_ptr<pag::PAGSurfaceExt> ext;
};

struct pag_image {
  explicit pag_image(std::shared_ptr<pag::PAGImage> image) : p(std::move(image)) {
  }

  PAGObjectType type = PAGObjectType::Image;
  std::shared_ptr<pag::PAGImage> p;
};

struct pag_font {
  explicit pag_font(pag::PAGFont font) : p(std::move(font)) {
  }

  PAGObjectType type = PAGObjectType::Font;
  pag::PAGFont p;
};

struct pag_backend_texture {
  explicit pag_backend_texture(const pag::BackendTexture& backendTexture) : p(backendTexture) {
  }
  explicit pag_backend_texture(void* otherBackend) : otherBackend(otherBackend) {
  }

  PAGObjectType type = PAGObjectType::BackendTexture;
  pag::BackendTexture p;
  void* otherBackend = nullptr;
};

struct pag_backend_semaphore {
  pag_backend_semaphore() = default;

  PAGObjectType type = PAGObjectType::BackendSemaphore;
  pag::BackendSemaphore p;
};

struct pag_decoder {
  explicit pag_decoder(std::shared_ptr<pag::PAGDecoder> decoder) : p(std::move(decoder)) {
  }

  PAGObjectType type = PAGObjectType::Decoder;
  std::shared_ptr<pag::PAGDecoder> p;
};

struct pag_animator {
  pag_animator() = default;

  PAGObjectType type = PAGObjectType::PAGAnimator;
  std::shared_ptr<pag::PAGAnimator> p;
  std::shared_ptr<pag::PAGAnimator::Listener> listener;
};

std::shared_ptr<pag::PAGFile> ToPAGFile(pag_file* file);

std::shared_ptr<pag::PAGComposition> ToPAGComposition(pag_composition* composition);

bool FromCTimeStretchMode(pag_time_stretch_mode cTimeStretchMode,
                          pag::PAGTimeStretchMode* timeStretchMode);

bool ToCTimeStretchMode(pag::PAGTimeStretchMode timeStretchMode,
                        pag_time_stretch_mode* cTimeStretchMode);

bool FromCColorType(pag_color_type cColorType, pag::ColorType* colorType);

bool ToCColorType(pag::ColorType colorType, pag_color_type* cColorType);

bool FromCAlphaType(pag_alpha_type cAlphaType, pag::AlphaType* alphaType);

bool ToCAlphaType(pag::AlphaType alphaType, pag_alpha_type* cAlphaType);

bool FromCScaleMode(pag_scale_mode cScaleMode, pag::PAGScaleMode* scaleMode);

bool ToCScaleMode(pag::PAGScaleMode scaleMode, pag_scale_mode* cScaleMode);

bool FromCParagraphJustification(pag_paragraph_justification cParagraphJustification,
                                 pag::ParagraphJustification* paragraphJustification);

bool ToCParagraphJustification(pag::ParagraphJustification paragraphJustification,
                               pag_paragraph_justification* cParagraphJustification);

bool FromCLayerType(pag_layer_type cLayerType, pag::LayerType* layerType);

bool TocLayerType(pag::LayerType layerType, pag_layer_type* cLayerType);

bool FromCImageOrigin(pag_image_origin cImageOrigin, pag::ImageOrigin* imageOrigin);

bool ToCImageOrigin(pag::ImageOrigin imageOrigin, pag_image_origin* cImageOrigin);
