/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include "pagx/PAGImage.h"
#include "base/utils/TGFXCast.h"
#include "pagx/utils/Base64.h"
#include "renderer/ToTGFX.h"
#include "tgfx/core/Image.h"
#include "tgfx/gpu/opengl/GLDevice.h"

namespace pagx {

std::shared_ptr<PAGImage> PAGImage::MakeFromPath(const std::string& path) {
  if (path.empty()) {
    return nullptr;
  }
  auto image = tgfx::Image::MakeFromFile(path);
  if (image == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGImage>(new PAGImage(std::move(image), path));
}

std::shared_ptr<PAGImage> PAGImage::MakeFromDataURI(const std::string& dataURI) {
  auto data = DecodeBase64DataURI(dataURI);
  if (data == nullptr) {
    return nullptr;
  }
  auto tgfxData = ToTGFXData(data);
  auto image = tgfx::Image::MakeFromEncoded(tgfxData);
  if (image == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGImage>(new PAGImage(std::move(image), dataURI));
}

std::shared_ptr<PAGImage> PAGImage::MakeFromData(const std::shared_ptr<Data>& data) {
  if (data == nullptr || data->size() == 0) {
    return nullptr;
  }
  auto tgfxData = ToTGFXData(data);
  auto image = tgfx::Image::MakeFromEncoded(tgfxData);
  if (image == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGImage>(new PAGImage(std::move(image), {}));
}

std::shared_ptr<PAGImage> PAGImage::MakeFromTexture(const pag::BackendTexture& texture,
                                                    pag::ImageOrigin origin) {
  if (!texture.isValid()) {
    return nullptr;
  }
  auto nativeHandle = tgfx::GLDevice::CurrentNativeHandle();
  if (nativeHandle == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<PAGImage>(new PAGImage(texture, origin));
}

void PAGImage::ensureTGFXImage() {
  if (_tgfxImage != nullptr || !_deferredTexture.isValid()) {
    return;
  }
  auto device = tgfx::GLDevice::Current();
  if (device == nullptr) {
    return;
  }
  auto* context = device->lockContext();
  if (context == nullptr) {
    return;
  }
  _tgfxImage =
      tgfx::Image::MakeFrom(context, pag::ToTGFX(_deferredTexture), pag::ToTGFX(_deferredOrigin));
  device->unlock();
}

const std::string& PAGImage::source() const {
  return _source;
}
PAGImage::PAGImage(std::shared_ptr<tgfx::Image> image, std::string source)
    : _tgfxImage(std::move(image)), _source(std::move(source)) {
}

PAGImage::PAGImage(pag::BackendTexture texture, pag::ImageOrigin origin)
    : _deferredTexture(std::move(texture)), _deferredOrigin(origin) {
}

}  // namespace pagx
