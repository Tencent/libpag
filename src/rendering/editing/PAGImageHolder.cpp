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

#include "PAGImageHolder.h"

namespace pag {
bool PAGImageHolder::hasImage(int editableIndex) const {
    return imageMap.count(editableIndex) > 0;
}

std::shared_ptr<PAGImage> PAGImageHolder::getImage(int editableIndex) const {
    auto result = imageMap.find(editableIndex);
    if (result != imageMap.end()) {
        return result->second;
    }
    return nullptr;
}

void PAGImageHolder::setImage(int editableIndex, std::shared_ptr<PAGImage> image) {
    if (image) {
        imageMap[editableIndex] = image;
    } else {
        imageMap.erase(editableIndex);
    }
}

void PAGImageHolder::addLayer(PAGLayer* layer) {
    auto position = std::find(imageLayers.begin(), imageLayers.end(), layer);
    if (position == imageLayers.end()) {
        imageLayers.push_back(layer);
    }
}

void PAGImageHolder::removeLayer(PAGLayer* layer) {
    auto position = std::find(imageLayers.begin(), imageLayers.end(), layer);
    if (position != imageLayers.end()) {
        imageLayers.erase(position);
    }
}

std::vector<PAGLayer*> PAGImageHolder::getLayers(int editableIndex) {
    std::vector<PAGLayer*> layers = {};
    for (auto& layer : imageLayers) {
        if (layer->editableIndex() == editableIndex) {
            layers.push_back(layer);
        }
    }
    return layers;
}
}  // namespace pag