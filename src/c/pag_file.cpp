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

#include "pag/c/pag_file.h"
#include "pag_types_priv.h"

pag_file* pag_file_load(const void* bytes, size_t length, const char* filePath) {
  std::string path(filePath);
  if (auto file = pag::PAGFile::Load(bytes, length, path)) {
    return new pag_file(std::move(file));
  }
  return nullptr;
}

void pag_file_set_duration(pag_file* file, int64_t duration) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return;
  }
  pagFile->setDuration(duration);
}

int pag_file_get_num_texts(pag_file* file) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->numTexts();
}

int pag_file_get_num_images(pag_file* file) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->numImages();
}

int pag_file_get_num_videos(pag_file* file) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->numVideos();
}

pag_time_stretch_mode pag_file_get_time_stretch_mode(pag_file* file) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return pag_time_stretch_mode_none;
  }
  pag_time_stretch_mode mode;
  if (ToCTimeStretchMode(pagFile->timeStretchMode(), &mode)) {
    return mode;
  }
  return pag_time_stretch_mode_none;
}

void pag_file_set_time_stretch_mode(pag_file* file, pag_time_stretch_mode mode) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return;
  }
  pag::PAGTimeStretchMode pagMode;
  if (FromCTimeStretchMode(mode, &pagMode)) {
    pagFile->setTimeStretchMode(pagMode);
  }
}

pag_text_document* pag_file_get_text_data(pag_file* file, int editableTextIndex) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return nullptr;
  }
  if (auto text = pagFile->getTextData(editableTextIndex)) {
    return new pag_text_document(std::move(text));
  }
  return nullptr;
}

void pag_file_replace_text(pag_file* file, int editableTextIndex, pag_text_document* text) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return;
  }
  if (text) {
    pagFile->replaceText(editableTextIndex, text->p);
  } else {
    pagFile->replaceText(editableTextIndex, nullptr);
  }
}

void pag_file_replace_image(pag_file* file, int editableImageIndex, pag_image* image) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return;
  }
  if (image) {
    pagFile->replaceImage(editableImageIndex, image->p);
  } else {
    pagFile->replaceImage(editableImageIndex, nullptr);
  }
}

pag_layer** pag_file_get_layers_by_editable_index(pag_file* file, int editableIndex,
                                                  pag_layer_type layerType, size_t* numLayers) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return nullptr;
  }

  pag::LayerType pagLayerType;
  FromCLayerType(layerType, &pagLayerType);

  auto pagLayers = pagFile->getLayersByEditableIndex(editableIndex, pagLayerType);
  if (pagLayers.empty()) {
    return nullptr;
  }
  auto** layers = static_cast<pag_layer**>(malloc(sizeof(pag_layer*) * pagLayers.size()));
  if (layers == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < pagLayers.size(); ++i) {
    layers[i] = new pag_layer(std::move(pagLayers[i]));
  }
  *numLayers = pagLayers.size();
  return layers;
}

int* pag_file_get_editable_indices(pag_file* file, pag_layer_type layerType, size_t* numIndexes) {
  auto pagFile = ToPAGFile(file);
  if (pagFile == nullptr) {
    return nullptr;
  }

  pag::LayerType pagLayerType;
  FromCLayerType(layerType, &pagLayerType);

  auto indexes = pagFile->getEditableIndices(pagLayerType);
  if (indexes.empty()) {
    return nullptr;
  }
  int* editableIndexes = static_cast<int*>(malloc(sizeof(int) * indexes.size()));
  if (editableIndexes == nullptr) {
    return nullptr;
  }
  for (size_t i = 0; i < indexes.size(); ++i) {
    editableIndexes[i] = indexes[i];
  }
  *numIndexes = indexes.size();
  return editableIndexes;
}
