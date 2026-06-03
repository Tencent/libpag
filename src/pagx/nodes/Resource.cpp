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

#include "pagx/nodes/ImageResource.h"
#include "pagx/nodes/FontResource.h"
#include "pagx/nodes/CompositionResource.h"

namespace pagx {

std::shared_ptr<ImageResource> ImageResource::FromBytes(const void* bytes, size_t length) {
  return FromData(Data::MakeWithCopy(bytes, length));
}

std::shared_ptr<ImageResource> ImageResource::FromData(std::shared_ptr<Data> data) {
  if (data == nullptr || data->empty()) {
    return nullptr;
  }
  return std::shared_ptr<ImageResource>(new ImageResource(std::move(data)));
}

std::shared_ptr<FontResource> FontResource::FromBytes(const void* bytes, size_t length,
                                                      int ttcIndex) {
  return FromData(Data::MakeWithCopy(bytes, length), ttcIndex);
}

std::shared_ptr<FontResource> FontResource::FromData(std::shared_ptr<Data> data, int ttcIndex) {
  if (data == nullptr || data->empty()) {
    return nullptr;
  }
  return std::shared_ptr<FontResource>(new FontResource(std::move(data), ttcIndex));
}

std::shared_ptr<FontResource> FontResource::MakeCustom(std::shared_ptr<FontProvider> provider) {
  if (provider == nullptr) {
    return nullptr;
  }
  return std::shared_ptr<FontResource>(new FontResource(std::move(provider)));
}

std::shared_ptr<CompositionResource> CompositionResource::FromBytes(const void* bytes,
                                                                    size_t length,
                                                                    const std::string& baseDir) {
  return FromData(Data::MakeWithCopy(bytes, length), baseDir);
}

std::shared_ptr<CompositionResource> CompositionResource::FromData(std::shared_ptr<Data> data,
                                                                   const std::string& baseDir) {
  if (data == nullptr || data->empty()) {
    return nullptr;
  }
  return std::shared_ptr<CompositionResource>(new CompositionResource(std::move(data), baseDir));
}

}  // namespace pagx
