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

#include "pag_types_priv.h"

static void DeletePAGByteData(const void* pointer) {
  delete reinterpret_cast<const pag_byte_data*>(pointer);
}

static void DeletePAGTextDocument(const void* pointer) {
  delete reinterpret_cast<const pag_text_document*>(pointer);
}

static void DeletePAGLayer(const void* pointer) {
  delete reinterpret_cast<const pag_layer*>(pointer);
}

static void DeletePAGPlayer(const void* pointer) {
  delete reinterpret_cast<const pag_player*>(pointer);
}

static void DeletePAGSurface(const void* pointer) {
  delete reinterpret_cast<const pag_surface*>(pointer);
}

static void DeletePAGImage(const void* pointer) {
  delete reinterpret_cast<const pag_image*>(pointer);
}

static void DeletePAGFont(const void* pointer) {
  delete reinterpret_cast<const pag_font*>(pointer);
}

static void DeletePAGBackendTexture(const void* pointer) {
  delete reinterpret_cast<const pag_backend_texture*>(pointer);
}

static void DeletePAGBackendSemaphore(const void* pointer) {
  delete reinterpret_cast<const pag_backend_semaphore*>(pointer);
}

static void DeletePAGDecoder(const void* pointer) {
  delete reinterpret_cast<const pag_decoder*>(pointer);
}

static void DeletePAGAnimator(const void* pointer) {
  delete reinterpret_cast<const pag_animator*>(pointer);
}

static void (*DeleteFunctions[])(const void*) = {
    DeletePAGByteData,         DeletePAGTextDocument, DeletePAGLayer,   DeletePAGPlayer,
    DeletePAGSurface,          DeletePAGImage,        DeletePAGFont,    DeletePAGBackendTexture,
    DeletePAGBackendSemaphore, DeletePAGDecoder,      DeletePAGAnimator};

void pag_release(pag_object object) {
  if (object == nullptr) {
    return;
  }
  auto type = *reinterpret_cast<const PAGObjectType*>(object);
  if (type == PAGObjectType::Unknown || type >= PAGObjectType::Count) {
    return;
  }
  DeleteFunctions[static_cast<uint8_t>(type) - 1](object);
}

namespace pag {
BackendTexture PAGSurfaceExt::getFrontTexture() {
  return surface->getFrontTexture();
}
BackendTexture PAGSurfaceExt::getBackTexture() {
  return surface->getBackTexture();
}
HardwareBufferRef PAGSurfaceExt::getFrontHardwareBuffer() {
  return surface->getFrontHardwareBuffer();
}
HardwareBufferRef PAGSurfaceExt::getBackHardwareBuffer() {
  return surface->getBackHardwareBuffer();
}
}  // namespace pag

std::shared_ptr<pag::PAGComposition> ToPAGComposition(pag_composition* composition) {
  if (composition == nullptr) {
    return nullptr;
  }
  return std::static_pointer_cast<pag::PAGComposition>(composition->p);
}

std::shared_ptr<pag::PAGFile> ToPAGFile(pag_file* file) {
  if (file == nullptr || !file->p->isPAGFile()) {
    return nullptr;
  }
  return std::static_pointer_cast<pag::PAGFile>(file->p);
}
const struct {
  pag_time_stretch_mode C;
  pag::PAGTimeStretchMode Pag;
} gTimeStretchPairs[] = {
    {pag_time_stretch_mode_none, pag::PAGTimeStretchMode::None},
    {pag_time_stretch_mode_scale, pag::PAGTimeStretchMode::Scale},
    {pag_time_stretch_mode_repeat_inverted, pag::PAGTimeStretchMode::Repeat},
    {pag_time_stretch_mode_repeat_inverted, pag::PAGTimeStretchMode::RepeatInverted},
};

bool FromCTimeStretchMode(pag_time_stretch_mode cTimeStretchMode,
                          pag::PAGTimeStretchMode* timeStretchMode) {
  for (auto& pair : gTimeStretchPairs) {
    if (pair.C == cTimeStretchMode) {
      *timeStretchMode = pair.Pag;
      return true;
    }
  }
  return false;
}

bool ToCTimeStretchMode(pag::PAGTimeStretchMode timeStretchMode,
                        pag_time_stretch_mode* cTimeStretchMode) {
  for (auto& pair : gTimeStretchPairs) {
    if (pair.Pag == timeStretchMode) {
      *cTimeStretchMode = pair.C;
      return true;
    }
  }
  return false;
}

const struct {
  pag_color_type C;
  pag::ColorType Pag;
} gColorTypePairs[] = {
    {pag_color_type_unknown, pag::ColorType::Unknown},
    {pag_color_type_alpha_8, pag::ColorType::ALPHA_8},
    {pag_color_type_rgba_8888, pag::ColorType::RGBA_8888},
    {pag_color_type_bgra_8888, pag::ColorType::BGRA_8888},
    {pag_color_type_rgb_565, pag::ColorType::RGB_565},
    {pag_color_type_gray_8, pag::ColorType::Gray_8},
    {pag_color_type_rgba_f16, pag::ColorType::RGBA_F16},
    {pag_color_type_rgba_101012, pag::ColorType::RGBA_1010102},
};

bool FromCColorType(pag_color_type cColorType, pag::ColorType* colorType) {
  for (auto& pair : gColorTypePairs) {
    if (pair.C == cColorType) {
      *colorType = pair.Pag;
      return true;
    }
  }
  return false;
}

bool ToCColorType(pag::ColorType colorType, pag_color_type* cColorType) {
  for (auto& pair : gColorTypePairs) {
    if (pair.Pag == colorType) {
      *cColorType = pair.C;
      return true;
    }
  }
  return false;
}

const struct {
  pag_alpha_type C;
  pag::AlphaType Pag;
} gAlphaTypePairs[] = {
    {pag_alpha_type_unknown, pag::AlphaType::Unknown},
    {pag_alpha_type_opaque, pag::AlphaType::Opaque},
    {pag_alpha_type_premultiplied, pag::AlphaType::Premultiplied},
    {pag_alpha_type_unpremultiplied, pag::AlphaType::Unpremultiplied},
};

bool FromCAlphaType(pag_alpha_type cAlphaType, pag::AlphaType* alphaType) {
  for (auto& pair : gAlphaTypePairs) {
    if (pair.C == cAlphaType) {
      *alphaType = pair.Pag;
      return true;
    }
  }
  return false;
}

bool ToCAlphaType(pag::AlphaType alphaType, pag_alpha_type* cAlphaType) {
  for (auto& pair : gAlphaTypePairs) {
    if (pair.Pag == alphaType) {
      *cAlphaType = pair.C;
      return true;
    }
  }
  return false;
}

const struct {
  pag_scale_mode C;
  pag::PAGScaleMode Pag;
} gScaleModePairs[] = {
    {pag_scale_mode_none, pag::PAGScaleMode::None},
    {pag_scale_mode_stretch, pag::PAGScaleMode::Stretch},
    {pag_scale_mode_letter_box, pag::PAGScaleMode::LetterBox},
    {pag_scale_mode_zoom, pag::PAGScaleMode::Zoom},
};

bool FromCScaleMode(pag_scale_mode cScaleMode, pag::PAGScaleMode* scaleMode) {
  for (auto& pair : gScaleModePairs) {
    if (pair.C == cScaleMode) {
      *scaleMode = pair.Pag;
      return true;
    }
  }
  return false;
}

bool ToCScaleMode(pag::PAGScaleMode scaleMode, pag_scale_mode* cScaleMode) {
  for (auto& pair : gScaleModePairs) {
    if (pair.Pag == scaleMode) {
      *cScaleMode = pair.C;
      return true;
    }
  }
  return false;
}

const struct {
  pag_paragraph_justification C;
  pag::ParagraphJustification Pag;
} gParagraphJustificationPairs[] = {
    {pag_paragraph_justification_left_justify, pag::ParagraphJustification::LeftJustify},
    {pag_paragraph_justification_center_justify, pag::ParagraphJustification::CenterJustify},
    {pag_paragraph_justification_right_justify, pag::ParagraphJustification::RightJustify},
    {pag_paragraph_justification_full_justify_last_line_left,
     pag::ParagraphJustification::FullJustifyLastLineLeft},
    {pag_paragraph_justification_full_justify_last_line_right,
     pag::ParagraphJustification::FullJustifyLastLineRight},
    {pag_paragraph_justification_full_justify_last_line_center,
     pag::ParagraphJustification::FullJustifyLastLineCenter},
    {pag_paragraph_justification_full_justify_last_line_full,
     pag::ParagraphJustification::FullJustifyLastLineFull},
};

bool FromCParagraphJustification(pag_paragraph_justification cParagraphJustification,
                                 pag::ParagraphJustification* paragraphJustification) {
  for (auto& pair : gParagraphJustificationPairs) {
    if (pair.C == cParagraphJustification) {
      *paragraphJustification = pair.Pag;
      return true;
    }
  }
  return false;
}

bool ToCParagraphJustification(pag::ParagraphJustification paragraphJustification,
                               pag_paragraph_justification* cParagraphJustification) {
  for (auto& pair : gParagraphJustificationPairs) {
    if (pair.Pag == paragraphJustification) {
      *cParagraphJustification = pair.C;
      return true;
    }
  }
  return false;
}

const struct {
  pag_layer_type C;
  pag::LayerType Pag;
} gLayerTypePairs[] = {
    {pag_layer_type_unknown, pag::LayerType::Unknown},
    {pag_layer_type_null, pag::LayerType::Null},
    {pag_layer_type_solid, pag::LayerType::Solid},
    {pag_layer_type_text, pag::LayerType::Text},
    {pag_layer_type_shape, pag::LayerType::Shape},
    {pag_layer_type_image, pag::LayerType::Image},
    {pag_layer_type_pre_compose, pag::LayerType::PreCompose},
    {pag_layer_type_camera, pag::LayerType::Camera},
};

bool FromCLayerType(pag_layer_type cLayerType, pag::LayerType* layerType) {
  for (auto& pair : gLayerTypePairs) {
    if (pair.C == cLayerType) {
      *layerType = pair.Pag;
      return true;
    }
  }
  return false;
}

bool TocLayerType(pag::LayerType layerType, pag_layer_type* cLayerType) {
  for (auto& pair : gLayerTypePairs) {
    if (pair.Pag == layerType) {
      *cLayerType = pair.C;
      return true;
    }
  }
  return false;
}

const struct {
  pag_image_origin C;
  pag::ImageOrigin Pag;
} gImageOriginPairs[] = {
    {pag_image_origin_top_left, pag::ImageOrigin::TopLeft},
    {pag_image_origin_bottom_left, pag::ImageOrigin::BottomLeft},
};

bool FromCImageOrigin(pag_image_origin cImageOrigin, pag::ImageOrigin* imageOrigin) {
  for (auto& pair : gImageOriginPairs) {
    if (pair.C == cImageOrigin) {
      *imageOrigin = pair.Pag;
      return true;
    }
  }
  return false;
}

bool ToCImageOrigin(pag::ImageOrigin imageOrigin, pag_image_origin* cImageOrigin) {
  for (auto& pair : gImageOriginPairs) {
    if (pair.Pag == imageOrigin) {
      *cImageOrigin = pair.C;
      return true;
    }
  }
  return false;
}
