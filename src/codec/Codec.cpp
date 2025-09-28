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

#include <unordered_map>
#include <unordered_set>
#include "CompressionAlgorithm.h"
#include "base/utils/USE.h"
#include "base/utils/Verify.h"
#include "codec/Version.h"
#include "codec/tags/FileTags.h"
#include "codec/tags/PerformanceTag.h"
#include "pag/file.h"

namespace pag {

static const uint8_t EncryptedVersion = 3;

static const uint8_t KnownVersion = 3;

static bool HasTrackMatte(TrackMatteType type) {
  switch (type) {
    case TrackMatteType::Alpha:
    case TrackMatteType::AlphaInverted:
    case TrackMatteType::Luma:
    case TrackMatteType::LumaInverted:
      return true;
    default:
      return false;
  }
}

uint16_t Codec::MaxSupportedTagLevel() {
  return static_cast<uint16_t>(TagCode::Count) - 1;
}

template <typename T>
ID GetReferenceID(T* item) {
  return item ? item->id : 0;
}

void Codec::InstallReferences(Layer* layer) {
  std::unordered_map<ID, MaskData*> maskMap;
  for (auto mask : layer->masks) {
    maskMap.insert(std::make_pair(mask->id, mask));
  }
  for (auto effect : layer->effects) {
    if (!effect->maskReferences.empty()) {
      for (auto i = static_cast<int>(effect->maskReferences.size() - 1); i >= 0; i--) {
        auto id = GetReferenceID(effect->maskReferences[i]);
        delete effect->maskReferences[i];
        auto result = maskMap.find(id);
        if (result != maskMap.end()) {
          effect->maskReferences[i] = result->second;
        } else {
          effect->maskReferences.erase(effect->maskReferences.begin() + i);
        }
      }
    }
  }
  if (layer->type() == LayerType::Text) {
    auto pathOption = static_cast<TextLayer*>(layer)->pathOption;
    if (pathOption && pathOption->path) {
      auto id = GetReferenceID(pathOption->path);
      delete pathOption->path;
      pathOption->path = nullptr;
      auto result = maskMap.find(id);
      if (result != maskMap.end()) {
        pathOption->path = result->second;
      }
    }
  }
}

void Codec::InstallReferences(const std::vector<Layer*>& layers) {
  std::unordered_map<ID, Layer*> layerMap;
  std::for_each(layers.begin(), layers.end(), [&layerMap](Layer* layer) {
    InstallReferences(layer);
    layerMap.insert(std::make_pair(layer->id, layer));
  });
  int index = 0;
  for (auto layer : layers) {
    if (layer->parent) {
      auto id = GetReferenceID(layer->parent);
      delete layer->parent;
      layer->parent = nullptr;
      auto result = layerMap.find(id);
      if (result != layerMap.end()) {
        layer->parent = result->second;
      }
    }
    if (index > 0 && HasTrackMatte(layer->trackMatteType)) {
      layer->trackMatteLayer = layers[index - 1];
    }
    for (auto effect : layer->effects) {
      auto displacementMap = static_cast<DisplacementMapEffect*>(effect);
      if (effect->type() == EffectType::DisplacementMap && displacementMap->displacementMapLayer) {
        auto id = GetReferenceID(displacementMap->displacementMapLayer);
        delete displacementMap->displacementMapLayer;
        displacementMap->displacementMapLayer = nullptr;
        auto result = layerMap.find(id);
        if (result != layerMap.end()) {
          displacementMap->displacementMapLayer = result->second;
        }
      }
    }
    index++;
  }
}

void Codec::InstallReferences(const std::vector<Composition*>& compositions) {
  std::unordered_map<ID, Composition*> compositionMap;
  for (auto composition : compositions) {
    compositionMap.insert(std::make_pair(composition->id, composition));
  }
  for (auto item : compositions) {
    if (item->type() == CompositionType::Vector) {
      for (auto layer : static_cast<VectorComposition*>(item)->layers) {
        layer->containingComposition = static_cast<VectorComposition*>(item);
        if (layer->type() == LayerType::PreCompose &&
            static_cast<PreComposeLayer*>(layer)->composition) {
          auto preComposeLayer = static_cast<PreComposeLayer*>(layer);
          auto id = GetReferenceID(preComposeLayer->composition);
          delete preComposeLayer->composition;
          preComposeLayer->composition = nullptr;
          auto result = compositionMap.find(id);
          if (result != compositionMap.end()) {
            preComposeLayer->composition = result->second;
          }
        }
      }
    }
  }
}

std::shared_ptr<File> Codec::VerifyAndMake(const std::vector<pag::Composition*>& compositions,
                                           const std::vector<pag::ImageBytes*>& images) {
  bool success = !compositions.empty();
  for (auto composition : compositions) {
    if (composition == nullptr || !composition->verify()) {
      success = false;
      break;
    }
  }
  for (auto& imageBytes : images) {
    if (imageBytes == nullptr || !imageBytes->verify()) {
      success = false;
      break;
    }
  }
  if (!success) {
    for (auto& composition : compositions) {
      delete composition;
    }
    for (auto& imageBytes : images) {
      delete imageBytes;
    }
    return nullptr;
  }
  auto file = new File(compositions, images);
  return std::shared_ptr<File>(file);
}

DecodeStream ReadBodyBytes(DecodeStream* stream) {
  DecodeStream emptyStream(stream->context);
  if (stream->length() < 11) {
    PAGThrowError(stream->context, "Length of PAG file is too short.");
    return emptyStream;
  }
  auto P = stream->readInt8();
  auto A = stream->readInt8();
  auto G = stream->readInt8();
  if (P != 'P' || A != 'A' || G != 'G') {
    PAGThrowError(stream->context, "Invalid PAG file header.");
    return emptyStream;
  }
  auto version = stream->readUint8();
  if (version == EncryptedVersion) {
    PAGThrowError(stream->context, "Encrypted PAG file");
    return emptyStream;
  }
  if (version > KnownVersion) {
    PAGThrowError(stream->context, "Invalid PAG file header.");
    return emptyStream;
  }
  auto bodyLength = stream->readUint32();
  auto compression = stream->readInt8();
  if (compression != CompressionAlgorithm::UNCOMPRESSED) {
    PAGThrowError(stream->context, "Invalid PAG file header.");
    return emptyStream;
  }
  bodyLength = std::min(bodyLength, stream->bytesAvailable());
  return stream->readBytes(bodyLength);
}

std::shared_ptr<File> Codec::Decode(const void* bytes, uint32_t byteLength,
                                    const std::string& filePath) {
  CodecContext context = {};
  DecodeStream stream(&context, reinterpret_cast<const uint8_t*>(bytes), byteLength);
  auto bodyBytes = ReadBodyBytes(&stream);
  if (context.hasException()) {
    return nullptr;
  }
  ReadTags(&bodyBytes, &context, ReadTagsOfFile);
  if (context.hasException()) {
    return nullptr;
  }
  InstallReferences(context.compositions);
  if (context.hasException()) {
    return nullptr;
  }

  // Verify 提前到使用之前，避免未经Verify导致使用时crash
  auto file = VerifyAndMake(context.releaseCompositions(), context.releaseImages());
  if (file == nullptr) {
    return nullptr;
  }

  UpdateFileAttributes(file, &context, filePath);
  return file;
}

std::unique_ptr<ByteData> Codec::Encode(std::shared_ptr<File> file) {
  return Codec::Encode(file, nullptr);
}

std::unique_ptr<ByteData> Codec::Encode(std::shared_ptr<File> file,
                                        std::shared_ptr<PerformanceData> performanceData) {
  CodecContext context = {};
  EncodeStream bodyBytes(&context);
  WriteTagsOfFile(&bodyBytes, file.get(), performanceData.get());

  EncodeStream fileBytes(&context);
  fileBytes.writeInt8('P');
  fileBytes.writeInt8('A');
  fileBytes.writeInt8('G');
  fileBytes.writeUint8(Version);
  fileBytes.writeUint32(bodyBytes.length());
  fileBytes.writeInt8(CompressionAlgorithm::UNCOMPRESSED);
  fileBytes.writeBytes(&bodyBytes);
  return fileBytes.release();
}

std::shared_ptr<PerformanceData> Codec::ReadPerformanceData(const void* bytes,
                                                            uint32_t byteLength) {
  CodecContext context = {};
  DecodeStream stream(&context, reinterpret_cast<const uint8_t*>(bytes), byteLength);
  auto bodyBytes = ReadBodyBytes(&stream);
  if (context.hasException()) {
    return nullptr;
  }
  auto header = ReadTagHeader(&bodyBytes);
  if (context.hasException()) {
    return nullptr;
  }

  while (header.code != TagCode::End) {
    auto tagBytes = bodyBytes.readBytes(header.length);
    if (header.code == TagCode::Performance) {
      auto data = std::shared_ptr<PerformanceData>(new PerformanceData());
      ReadPerformanceTag(&tagBytes, data.get());
      return data;
    }
    header = ReadTagHeader(&bodyBytes);
    if (context.hasException()) {
      return nullptr;
    }
  }
  return nullptr;
}

void Codec::UpdateFileAttributes(std::shared_ptr<File> file, CodecContext* context,
                                 const std::string& filePath) {
  for (auto& composition : file->compositions) {
    if (!composition->staticTimeRangeUpdated) {
      composition->updateStaticTimeRanges();
      composition->staticTimeRangeUpdated = true;
    }
  }

  if (context->scaledTimeRange != nullptr) {
    file->scaledTimeRange.start =
        std::max(static_cast<int64_t>(0), context->scaledTimeRange->start);
    file->scaledTimeRange.end = std::min(file->duration(), context->scaledTimeRange->end);
  }
  file->path = filePath;
  file->_tagLevel = context->tagLevel;
  file->timeStretchMode = context->timeStretchMode;
  file->fileAttributes = context->fileAttributes;
  file->editableImages = context->editableImages;
  file->editableTexts = context->editableTexts;
  file->imageScaleModes = context->imageScaleModes;
  if (file->editableImages == nullptr) {
    auto maxIndex = file->numImages();
    file->editableImages = new std::vector<int>(maxIndex);
    for (int i = 0; i < maxIndex; i++) {
      file->editableImages->at(i) = i;
    }
  }
  if (file->editableTexts == nullptr) {
    auto maxIndex = file->numTexts();
    file->editableTexts = new std::vector<int>(maxIndex);
    for (int i = 0; i < maxIndex; i++) {
      file->editableTexts->at(i) = i;
    }
  }
}
}  // namespace pag
