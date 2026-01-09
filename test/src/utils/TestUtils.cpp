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

#include "TestUtils.h"
#include <fstream>
#include "base/utils/TGFXCast.h"
#include "rendering/utils/Directory.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/core/Stream.h"
#include "utils/ProjectPath.h"
#include "utils/TestDir.h"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GLES3/gl3.h>
#endif

namespace pag {
using namespace tgfx;

std::string ToString(Frame frame) {
  auto result = std::to_string(frame);
  while (result.size() < 4) {
    result = "0" + result;
  }
  return result;
}

BackendTexture ToBackendTexture(const tgfx::GLTextureInfo& texture, int width, int height) {
  GLTextureInfo glInfo = {};
  glInfo.id = texture.id;
  glInfo.target = texture.target;
  glInfo.format = texture.format;
  return {glInfo, width, height};
}

std::vector<std::string> GetAllPAGFiles(const std::string& path) {
  return Directory::FindFiles(ProjectPath::Absolute(path), ".pag");
}

Bitmap MakeSnapshot(std::shared_ptr<PAGSurface> pagSurface) {
  Bitmap bitmap(pagSurface->width(), pagSurface->height(), false, false);
  if (bitmap.isEmpty()) {
    return {};
  }
  Pixmap pixmap(bitmap);
  auto result = pagSurface->readPixels(ToPAG(pixmap.colorType()), ToPAG(pixmap.alphaType()),
                                       pixmap.writablePixels(), pixmap.rowBytes());
  if (!result) {
    return {};
  }
  return bitmap;
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

bool CreateGLTexture(Context*, int width, int height, tgfx::GLTextureInfo* texture) {
  texture->target = GL_TEXTURE_2D;
  texture->format = GL_RGBA8;
  glGenTextures(1, &texture->id);
  if (texture->id <= 0) {
    return false;
  }
  glBindTexture(texture->target, texture->id);
  glTexParameteri(texture->target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture->target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexParameteri(texture->target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(texture->target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(texture->target, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glBindTexture(texture->target, 0);
  return true;
}

std::shared_ptr<PAGFile> LoadPAGFile(const std::string& path) {
  return PAGFile::Load(ProjectPath::Absolute(path));
}

std::shared_ptr<tgfx::ImageCodec> MakeImageCodec(const std::string& path) {
  return ImageCodec::MakeFrom(ProjectPath::Absolute(path));
}

std::shared_ptr<tgfx::Image> MakeImage(const std::string& path) {
  return Image::MakeFromFile(ProjectPath::Absolute(path));
}

std::shared_ptr<PAGImage> MakePAGImage(const std::string& path) {
  return PAGImage::FromPath(ProjectPath::Absolute(path));
}

std::shared_ptr<tgfx::Data> ReadFile(const std::string& path) {
  auto stream = tgfx::Stream::MakeFromFile(ProjectPath::Absolute(path));
  if (stream == nullptr) {
    return nullptr;
  }
  tgfx::Buffer buffer(stream->size());
  stream->read(buffer.data(), buffer.size());
  return buffer.release();
}

std::string GetOutputFile(const std::string& key) {
#ifdef GENERATE_BASELINE_IMAGES
  static const std::string OUT_ROOT = TestDir::GetRoot() + "/baseline-out";
#else
  static const std::string OUT_ROOT = TestDir::GetRoot() + "/out";
#endif
  return OUT_ROOT + "/" + key + ".webp";
}

void SaveImage(const tgfx::Bitmap& bitmap, const std::string& key) {
  if (bitmap.isEmpty()) {
    return;
  }
  SaveImage(Pixmap(bitmap), key);
}

void SaveImage(const Pixmap& pixmap, const std::string& key) {
  auto data = ImageCodec::Encode(pixmap, EncodedFormat::WEBP, 100);
  if (data == nullptr) {
    return;
  }
  std::filesystem::path path = GetOutputFile(key);
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path);
  out.write(reinterpret_cast<const char*>(data->data()),
            static_cast<std::streamsize>(data->size()));
  out.close();
}

void RemoveImage(const std::string& key) {
  auto path = GetOutputFile(key);
  std::filesystem::remove(path);
}
}  // namespace pag
