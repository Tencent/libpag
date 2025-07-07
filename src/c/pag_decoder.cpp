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

#include "pag/c/pag_decoder.h"
#include "pag_types_priv.h"

pag_decoder* pag_decoder_create(pag_composition* composition, float maxFrameRate, float scale) {
  if (composition == nullptr) {
    return nullptr;
  }
  if (auto pagComposition = ToPAGComposition(composition)) {
    if (auto decoder = pag::PAGDecoder::MakeFrom(pagComposition, maxFrameRate, scale)) {
      return new pag_decoder(std::move(decoder));
    }
  }
  return nullptr;
}

int pag_decoder_get_width(pag_decoder* decoder) {
  if (decoder == nullptr) {
    return 0;
  }
  return decoder->p->width();
}

int pag_decoder_get_height(pag_decoder* decoder) {
  if (decoder == nullptr) {
    return 0;
  }
  return decoder->p->height();
}

int pag_decoder_get_num_frames(pag_decoder* decoder) {
  if (decoder == nullptr) {
    return 0;
  }
  return decoder->p->numFrames();
}

float pag_decoder_get_frame_rate(pag_decoder* decoder) {
  if (decoder == nullptr) {
    return 0.0f;
  }
  return decoder->p->frameRate();
}

bool pag_decoder_check_frame_changed(pag_decoder* decoder, int index) {
  if (decoder == nullptr) {
    return false;
  }
  return decoder->p->checkFrameChanged(index);
}

bool pag_decoder_read_frame(pag_decoder* decoder, int index, void* pixels, size_t rowBytes,
                            pag_color_type colorType, pag_alpha_type alphaType) {
  if (decoder == nullptr) {
    return false;
  }
  pag::ColorType color;
  if (!FromCColorType(colorType, &color)) {
    return false;
  }
  pag::AlphaType alpha;
  if (!FromCAlphaType(alphaType, &alpha)) {
    return false;
  }
  return decoder->p->readFrame(index, pixels, rowBytes, color, alpha);
}

bool pag_decoder_read_frame_to_hardware_buffer(pag_decoder* decoder, int index, void* buffer) {
  if (decoder == nullptr) {
    return false;
  }
  if (buffer == nullptr) {
    return false;
  }
  return decoder->p->readFrame(index, static_cast<pag::HardwareBufferRef>(buffer));
}
