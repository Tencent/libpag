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

#include "ReplacementHolder.h"

namespace pag {
template <class T>
bool ReplacementHolder<T>::hasReplacement(int editableIndex) const {
  return replacementMap.count(editableIndex) > 0;
}

template <class T>
std::shared_ptr<T> ReplacementHolder<T>::getReplacement(int editableIndex) const {
  auto result = replacementMap.find(editableIndex);
  if (result != replacementMap.end()) {
    return result->second;
  }
  return nullptr;
}

template <class T>
void ReplacementHolder<T>::setReplacement(int editableIndex, std::shared_ptr<T> replacement,
                                          PAGLayer* layerOwner) {
  if (replacement) {
    replacementMap[editableIndex] = replacement;
    ownerMap[editableIndex] = layerOwner;
  } else {
    replacementMap.erase(editableIndex);
    ownerMap.erase(editableIndex);
  }
}

template <class T>
PAGLayer* ReplacementHolder<T>::getOwner(int editableIndex) const {
  auto result = ownerMap.find(editableIndex);
  if (result != ownerMap.end()) {
    return result->second;
  }
  return nullptr;
}

template <class T>
void ReplacementHolder<T>::addLayer(PAGLayer* layer) {
  auto position = std::find(editableLayers.begin(), editableLayers.end(), layer);
  if (position == editableLayers.end()) {
    editableLayers.push_back(layer);
  }
}

template <class T>
void ReplacementHolder<T>::removeLayer(PAGLayer* layer) {
  auto position = std::find(editableLayers.begin(), editableLayers.end(), layer);
  if (position != editableLayers.end()) {
    editableLayers.erase(position);
  }
}

template <class T>
std::vector<PAGLayer*> ReplacementHolder<T>::getLayers(int editableIndex) {
  std::vector<PAGLayer*> layers = {};
  for (auto& layer : editableLayers) {
    if (layer->editableIndex() == editableIndex) {
      layers.push_back(layer);
    }
  }
  return layers;
}

template class ReplacementHolder<PAGImage>;
template class ReplacementHolder<TextDocument>;
template class ReplacementHolder<Color>;
}  // namespace pag