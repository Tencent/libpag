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

#include "PAGTestUtils.h"
#include <dirent.h>
#include "base/utils/TGFXCast.h"
#include "tgfx/core/Image.h"
#include "tgfx/gpu/opengl/GLFunctions.h"

namespace pag {
using namespace tgfx;

std::string ToString(Frame frame) {
  auto result = std::to_string(frame);
  while (result.size() < 4) {
    result = "0" + result;
  }
  return result;
}

void GetAllPAGFiles(std::string path, std::vector<std::string>& files) {
  struct dirent* dirp;
  DIR* dir = opendir(path.c_str());
  std::string p;

  while ((dirp = readdir(dir)) != nullptr) {
    if (dirp->d_type == DT_REG) {
      std::string str(dirp->d_name);
      std::string::size_type idx = str.find(".pag");
      if (idx != std::string::npos) {
        files.push_back(p.assign(path).append("/").append(dirp->d_name));
      }
    }
  }
  closedir(dir);
}

std::shared_ptr<PixelBuffer> MakeSnapshot(std::shared_ptr<PAGSurface> pagSurface) {
  auto pixelBuffer = PixelBuffer::Make(pagSurface->width(), pagSurface->height(), false, false);
  if (pixelBuffer == nullptr) {
    return nullptr;
  }
  Bitmap bitmap(pixelBuffer);
  auto result = pagSurface->readPixels(ToPAG(bitmap.colorType()), ToPAG(bitmap.alphaType()),
                                       bitmap.writablePixels(), bitmap.rowBytes());
  return result ? pixelBuffer : nullptr;
}

std::shared_ptr<PAGLayer> GetLayer(std::shared_ptr<PAGComposition> root, LayerType type,
                                   int& targetIndex) {
  if (root == nullptr) return nullptr;
  if (type == LayerType::PreCompose) {
    if (targetIndex == 0) {
      return root;
    }
    targetIndex--;
  }
  for (int i = 0; i < root->numChildren(); i++) {
    auto layer = root->getLayerAt(i);
    if (layer->layerType() == type) {
      if (targetIndex == 0) {
        return layer;
      }
      targetIndex--;
    } else if (layer->layerType() == LayerType::PreCompose) {
      return GetLayer(std::static_pointer_cast<PAGComposition>(layer), type, targetIndex);
    }
  }
  return nullptr;
}

bool CreateGLTexture(Context* context, int width, int height, GLSampler* texture) {
  texture->target = GL_TEXTURE_2D;
  texture->format = PixelFormat::RGBA_8888;
  auto gl = GLFunctions::Get(context);
  gl->genTextures(1, &texture->id);
  if (texture->id <= 0) {
    return false;
  }
  gl->bindTexture(texture->target, texture->id);
  gl->texParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  gl->texParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gl->texParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  gl->texParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->texImage2D(texture->target, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  gl->bindTexture(texture->target, 0);
  return true;
}
}  // namespace pag