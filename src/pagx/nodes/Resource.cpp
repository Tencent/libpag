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

#include "pagx/nodes/Resource.h"
#include <algorithm>
#include <utility>
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/CompositionResource.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/FontResource.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImageResource.h"
#include "pagx/nodes/Layer.h"

namespace pagx {

void Resource::addReferencer(ResourceReferencer* referencer) {
  if (referencer == nullptr) {
    return;
  }
  if (std::find(resourceReferencers.begin(), resourceReferencers.end(), referencer) ==
      resourceReferencers.end()) {
    resourceReferencers.push_back(referencer);
  }
}

void Resource::removeReferencer(ResourceReferencer* referencer) {
  resourceReferencers.erase(
      std::remove(resourceReferencers.begin(), resourceReferencers.end(), referencer),
      resourceReferencers.end());
}

void Resource::notifyUpdated() {
  auto referencers = resourceReferencers;
  for (auto* referencer : referencers) {
    if (referencer != nullptr) {
      referencer->resourceUpdated(this);
    }
  }
}

ResourceReferencer::~ResourceReferencer() {
  if (retainedResource != nullptr) {
    retainedResource->removeReferencer(this);
  }
}

void ResourceReferencer::setResource(std::shared_ptr<Resource> resource) {
  if (retainedResource != nullptr) {
    retainedResource->removeReferencer(this);
  }
  retainedResource = std::move(resource);
  if (retainedResource != nullptr) {
    retainedResource->addReferencer(this);
  }
}

std::shared_ptr<ImageResource> ImageResource::MakeEmpty() {
  return std::shared_ptr<ImageResource>(new ImageResource());
}

std::shared_ptr<ImageResource> ImageResource::FromBytes(const void* bytes, size_t length) {
  return FromData(Data::MakeWithCopy(bytes, length));
}

std::shared_ptr<ImageResource> ImageResource::FromData(std::shared_ptr<Data> data) {
  if (data == nullptr || data->empty()) {
    return nullptr;
  }
  return std::shared_ptr<ImageResource>(new ImageResource(std::move(data)));
}

bool ImageResource::setData(std::shared_ptr<Data> data) {
  if (data == nullptr || data->empty()) {
    return false;
  }
  imageData = std::move(data);
  notifyUpdated();
  return true;
}

bool ImageResource::setBytes(const void* bytes, size_t length) {
  return setData(Data::MakeWithCopy(bytes, length));
}

std::shared_ptr<FontResource> FontResource::MakeEmpty() {
  return std::shared_ptr<FontResource>(new FontResource());
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

bool FontResource::setData(std::shared_ptr<Data> data, int ttcIndex) {
  if (data == nullptr || data->empty()) {
    return false;
  }
  fontData = std::move(data);
  fontTtcIndex = ttcIndex;
  fontProvider = nullptr;
  notifyUpdated();
  return true;
}

bool FontResource::setBytes(const void* bytes, size_t length, int ttcIndex) {
  return setData(Data::MakeWithCopy(bytes, length), ttcIndex);
}

bool FontResource::setProvider(std::shared_ptr<FontProvider> provider) {
  if (provider == nullptr) {
    return false;
  }
  fontData = nullptr;
  fontTtcIndex = 0;
  fontProvider = std::move(provider);
  notifyUpdated();
  return true;
}

std::shared_ptr<CompositionResource> CompositionResource::MakeEmpty() {
  return std::shared_ptr<CompositionResource>(new CompositionResource());
}

std::shared_ptr<CompositionResource> CompositionResource::FromBytes(const void* bytes,
                                                                    size_t length) {
  return FromData(Data::MakeWithCopy(bytes, length));
}

std::shared_ptr<CompositionResource> CompositionResource::FromData(std::shared_ptr<Data> data) {
  if (data == nullptr || data->empty()) {
    return nullptr;
  }
  return std::shared_ptr<CompositionResource>(new CompositionResource(std::move(data)));
}

bool CompositionResource::setData(std::shared_ptr<Data> data) {
  if (data == nullptr || data->empty()) {
    return false;
  }
  compositionData = std::move(data);
  notifyUpdated();
  return true;
}

bool CompositionResource::setBytes(const void* bytes, size_t length) {
  return setData(Data::MakeWithCopy(bytes, length));
}

void Image::resourceUpdated(Resource* resource) {
  if (resource == nullptr || resource->resourceType() != ResourceType::Image) {
    return;
  }
  auto imageResource = static_cast<ImageResource*>(resource);
  if (imageResource == nullptr || imageResource->data() == nullptr) {
    return;
  }
  data = imageResource->data();
  if (ownerDocument != nullptr) {
    ownerDocument->notifyChange({this});
  }
}

void Font::resourceUpdated(Resource* resource) {
  if (resource == nullptr || resource->resourceType() != ResourceType::Font) {
    return;
  }
  auto fontResource = static_cast<FontResource*>(resource);
  if (fontResource == nullptr ||
      (fontResource->data() == nullptr && fontResource->provider() == nullptr)) {
    return;
  }
  data = fontResource->data();
  ttcIndex = fontResource->ttcIndex();
  provider = fontResource->provider();
  if (ownerDocument != nullptr) {
    ownerDocument->notifyChange({this});
  }
}

void Layer::resourceUpdated(Resource* resource) {
  if (resource == nullptr || resource->resourceType() != ResourceType::Composition ||
      ownerDocument == nullptr) {
    return;
  }
  auto compositionResource = static_cast<CompositionResource*>(resource);
  if (compositionResource == nullptr || compositionResource->data() == nullptr) {
    return;
  }
  auto data = compositionResource->data();
  auto externalDocument = PAGXImporter::FromXML(data->bytes(), data->size());
  if (externalDocument == nullptr) {
    return;
  }
  if (composition == nullptr) {
    composition = ownerDocument->makeNode<Composition>();
  }
  for (const auto& error : externalDocument->errors) {
    ownerDocument->errors.push_back(error);
  }
  composition->width = externalDocument->width;
  composition->height = externalDocument->height;
  composition->layers = externalDocument->layers;
  composition->animations = externalDocument->animations;
  composition->externalDoc = externalDocument;
  externalDoc = externalDocument;
  compositionFilePath = {};
  ownerDocument->notifyChange({this});
}

}  // namespace pagx
