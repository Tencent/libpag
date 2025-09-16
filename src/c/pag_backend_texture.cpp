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

#include "pag/c/pag_backend_texture.h"
#include "pag_types_priv.h"

using namespace pag;

pag_backend_texture* pag_backend_texture_create_from_gl_texture_info(
    pag_gl_texture_info textureInfo, int width, int height) {
  pag::GLTextureInfo info;
  info.id = textureInfo.id;
  info.target = textureInfo.target;
  info.format = textureInfo.format;
  pag::BackendTexture backendTexture(info, width, height);
  return new pag_backend_texture(backendTexture);
}

bool pag_backend_texture_get_gl_texture_info(pag_backend_texture* texture,
                                             pag_gl_texture_info* textureInfo) {
  if (texture == nullptr || textureInfo == nullptr) {
    return false;
  }
  GLTextureInfo info;
  if (texture->p.getGLTextureInfo(&info)) {
    textureInfo->id = info.id;
    textureInfo->target = info.target;
    textureInfo->format = info.format;
    return true;
  }
  return false;
}

bool pag_backend_texture_get_vk_image_info(pag_backend_texture* texture,
                                           pag_vk_image_info* imageInfo) {
  if (texture == nullptr || imageInfo == nullptr) {
    return false;
  }
  VkImageInfo info;
  if (texture->p.getVkImageInfo(&info)) {
    imageInfo->image = info.image;
    return true;
  }
  return false;
}

bool pag_backend_texture_get_mtl_texture_info(pag_backend_texture* texture,
                                              pag_mtl_texture_info* mtl_texture_info) {
  if (texture == nullptr || mtl_texture_info == nullptr) {
    return false;
  }
  MtlTextureInfo info;
  if (texture->p.getMtlTextureInfo(&info)) {
    mtl_texture_info->texture = info.texture;
    return true;
  }
  return false;
}
