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

#include "pag/file.h"
#include <algorithm>
#include <unordered_map>

namespace pag {

static std::mutex globalLocker = {};
static std::unordered_map<std::string, std::weak_ptr<File>> weakFileMap =
    std::unordered_map<std::string, std::weak_ptr<File>>();

static std::shared_ptr<File> FindFileByPath(const std::string& filePath) {
  std::lock_guard<std::mutex> autoLock(globalLocker);
  if (filePath.empty()) {
    return nullptr;
  }
  auto result = weakFileMap.find(filePath);
  if (result != weakFileMap.end()) {
    auto& weak = result->second;
    auto file = weak.lock();
    if (file) {
      return file;
    }
    weakFileMap.erase(result);
    if (weakFileMap.size() > 50) {  // do cleaning.
      std::vector<std::string> needRemoveList = {};
      for (auto& item : weakFileMap) {
        if (item.second.expired()) {
          needRemoveList.push_back(item.first);
        }
      }
      for (auto& item : needRemoveList) {
        weakFileMap.erase(item);
      }
    }
  }
  return nullptr;
}

std::shared_ptr<File> File::Load(const std::string& filePath) {
  auto byteData = ByteData::FromPath(filePath);
  if (byteData == nullptr) {
    return nullptr;
  }
  return pag::File::Load(byteData->data(), byteData->length(), filePath);
}

std::shared_ptr<File> File::Load(const void* bytes, size_t length, const std::string& filePath) {
  auto file = FindFileByPath(filePath);
  if (file != nullptr) {
    return file;
  }
  file = Codec::Decode(bytes, static_cast<uint32_t>(length), filePath);
  if (file != nullptr) {
    std::lock_guard<std::mutex> autoLock(globalLocker);
    std::weak_ptr<File> weak = file;
    weakFileMap.insert(std::make_pair(filePath, std::move(weak)));
  }
  return file;
}

uint16_t File::MaxSupportedTagLevel() {
  return Codec::MaxSupportedTagLevel();
}

File::File(std::vector<Composition*> compositionList, std::vector<pag::ImageBytes*> imageList)
    : images(std::move(imageList)), compositions(std::move(compositionList)) {
  mainComposition = compositions.back();
  scaledTimeRange.start = 0;
  scaledTimeRange.end = mainComposition->duration;
  rootLayer = PreComposeLayer::Wrap(mainComposition).release();
  updateEditables(mainComposition);
  for (auto composition : compositions) {
    if (composition->type() != CompositionType::Vector) {
      _numLayers++;
      continue;
    }
    for (auto layer : static_cast<VectorComposition*>(composition)->layers) {
      if (layer->type() == LayerType::PreCompose) {
        continue;
      }
      _numLayers++;
    }
  }
}

File::~File() {
  for (auto& composition : compositions) {
    delete composition;
  }
  for (auto& imageBytes : images) {
    delete imageBytes;
  }
  delete rootLayer;
  delete editableImages;
  delete editableTexts;
  delete imageScaleModes;
}

void File::updateEditables(Composition* composition) {
  if (composition->type() != CompositionType::Vector) {
    return;
  }
  for (auto layer : static_cast<VectorComposition*>(composition)->layers) {
    if (layer->type() == LayerType::Text) {
      auto textLayer = static_cast<TextLayer*>(layer);
      // 多个预合成图层指向同一个预合成时，这个预合成里的文本图层需要排重.
      auto result = std::find(textLayers.begin(), textLayers.end(), textLayer);
      if (result == textLayers.end()) {
        textLayers.push_back(textLayer);
      }
    } else if (layer->type() == LayerType::Image) {
      auto imageLayer = static_cast<ImageLayer*>(layer);
      auto imageBytes = imageLayer->imageBytes;
      bool found = false;
      for (auto& list : imageLayers) {
        if (list[0]->imageBytes == imageBytes) {
          list.push_back(imageLayer);
          found = true;
          break;
        }
      }
      if (!found) {
        std::vector<ImageLayer*> list = {imageLayer};
        imageLayers.push_back(list);
      }
    } else if (layer->type() == LayerType::PreCompose) {
      updateEditables(static_cast<PreComposeLayer*>(layer)->composition);
    }
  }
}

int64_t File::duration() const {
  return mainComposition->duration;
}

float File::frameRate() const {
  return mainComposition->frameRate;
}

Color File::backgroundColor() const {
  return mainComposition->backgroundColor;
}

int32_t File::width() const {
  return mainComposition->width;
}

int32_t File::height() const {
  return mainComposition->height;
}

uint16_t File::tagLevel() const {
  return _tagLevel;
}

int File::numLayers() const {
  return _numLayers;
}

int File::numTexts() const {
  return static_cast<int>(textLayers.size());
}

int File::numImages() const {
  return static_cast<int>(imageLayers.size());
}

int File::numVideos() const {
  unsigned int count = 0;
  for (auto composition : compositions) {
    if (composition->type() == CompositionType::Video) {
      count++;
    }
  }
  return static_cast<int>(count);
}

TextDocumentHandle File::getTextData(int index) const {
  if (index < 0 || static_cast<size_t>(index) >= textLayers.size()) {
    return nullptr;
  }
  auto textDocument = textLayers[index]->getTextDocument();
  auto textData = new TextDocument();
  *textData = *textDocument;
  return TextDocumentHandle(textData);
}

PreComposeLayer* File::getRootLayer() const {
  return rootLayer;
}

TextLayer* File::getTextAt(int index) const {
  if (index < 0 || static_cast<size_t>(index) >= textLayers.size()) {
    return nullptr;
  }
  return textLayers[index];
}

int File::getEditableIndex(TextLayer* textLayer) const {
  auto result = std::find(textLayers.begin(), textLayers.end(), textLayer);
  if (result != textLayers.end()) {
    return static_cast<int>(result - textLayers.begin());
  }
  return -1;
}

int File::getEditableIndex(pag::ImageLayer* imageLayer) const {
  int index = 0;
  for (auto& layers : imageLayers) {
    auto result = std::find(layers.begin(), layers.end(), imageLayer);
    if (result != layers.end()) {
      return index;
    }
    index++;
  }
  return -1;
}

std::vector<ImageLayer*> File::getImageAt(int index) const {
  if (index < 0 || static_cast<size_t>(index) >= imageLayers.size()) {
    return {};
  }
  return imageLayers[index];
}

bool File::hasScaledTimeRange() const {
  return scaledTimeRange.start != 0 || scaledTimeRange.end != mainComposition->duration;
}
}  // namespace pag
