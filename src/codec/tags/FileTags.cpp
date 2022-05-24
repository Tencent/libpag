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

#include "FileTags.h"
#include <unordered_set>
#include "base/utils/EnumClassHash.h"
#include "codec/tags/BitmapCompositionTag.h"
#include "codec/tags/EditableIndices.h"
#include "codec/tags/FileAttributes.h"
#include "codec/tags/FontTables.h"
#include "codec/tags/Images.h"
#include "codec/tags/PerformanceTag.h"
#include "codec/tags/TimeStretchMode.h"
#include "codec/tags/VectorCompositionTag.h"
#include "codec/tags/VideoCompositionTag.h"

namespace pag {
static void ReadTag_FontTables(DecodeStream* stream, CodecContext*) {
  ReadFontTables(stream);
}

static void ReadTag_FileAttributes(DecodeStream* stream, CodecContext* context) {
  ReadFileAttributes(stream, &(context->fileAttributes));
}

static void ReadTag_TimeStretchMode(DecodeStream* stream, CodecContext* context) {
  ReadTimeStretchMode(stream, context);
}

static void ReadTag_ImageTables(DecodeStream* stream, CodecContext* context) {
  ReadImageTables(stream, &(context->images));
}

static void ReadTag_ImageBytes(DecodeStream* stream, CodecContext* context) {
  auto imageBytes = ReadImageBytes(stream);
  context->images.push_back(imageBytes);
}

static void ReadTag_ImageBytesV2(DecodeStream* stream, CodecContext* context) {
  auto imageBytes = ReadImageBytesV2(stream);
  context->images.push_back(imageBytes);
}

static void ReadTag_ImageBytesV3(DecodeStream* stream, CodecContext* context) {
  auto imageBytes = ReadImageBytesV3(stream);
  context->images.push_back(imageBytes);
}

static void ReadTag_VectorCompositionBlock(DecodeStream* stream, CodecContext* context) {
  auto composition = ReadVectorComposition(stream);
  context->compositions.push_back(composition);
}

static void ReadTag_BitmapCompositionBlock(DecodeStream* stream, CodecContext* context) {
  auto composition = ReadBitmapComposition(stream);
  context->compositions.push_back(composition);
}

static void ReadTag_VideoCompositionBlock(DecodeStream* stream, CodecContext* context) {
  auto composition = ReadVideoComposition(stream);
  context->compositions.push_back(composition);
}

static void ReadTag_EditableIndicesBlock(DecodeStream* stream, CodecContext*) {
  ReadEditableIndices(stream);
}

using ReadTagHandler = void(DecodeStream* stream, CodecContext* context);
static const std::unordered_map<TagCode, std::function<ReadTagHandler>, EnumClassHash> handlers = {
    {TagCode::FontTables, ReadTag_FontTables},
    {TagCode::FileAttributes, ReadTag_FileAttributes},
    {TagCode::TimeStretchMode, ReadTag_TimeStretchMode},
    {TagCode::ImageTables, ReadTag_ImageTables},
    {TagCode::ImageBytes, ReadTag_ImageBytes},
    {TagCode::ImageBytesV2, ReadTag_ImageBytesV2},
    {TagCode::ImageBytesV3, ReadTag_ImageBytesV3},
    {TagCode::VectorCompositionBlock, ReadTag_VectorCompositionBlock},
    {TagCode::BitmapCompositionBlock, ReadTag_BitmapCompositionBlock},
    {TagCode::VideoCompositionBlock, ReadTag_VideoCompositionBlock},
    {TagCode::EditableIndices, ReadTag_EditableIndicesBlock},
};

void ReadTagsOfFile(DecodeStream* stream, TagCode code, CodecContext* context) {
  auto iter = handlers.find(code);
  if (iter != handlers.end()) {
    iter->second(stream, context);
  }
}

void GetFontFromTextDocument(std::vector<FontData>& fontList,
                             std::unordered_set<std::string>& fontSet,
                             const TextDocumentHandle& textDocument) {
  if (textDocument == nullptr) {
    return;
  }
  auto key = textDocument->fontFamily + "|" + textDocument->fontStyle;
  if (fontSet.count(key) > 0) {
    return;
  }
  fontSet.insert(key);
  fontList.emplace_back(textDocument->fontFamily, textDocument->fontStyle);
}

std::vector<FontData> GetFontList(std::vector<Composition*> compositions) {
  std::vector<FontData> fontList;
  std::unordered_set<std::string> fontSet;
  for (auto& composition : compositions) {
    if (composition->type() != CompositionType::Vector) {
      continue;
    }
    for (auto& layer : reinterpret_cast<VectorComposition*>(composition)->layers) {
      if (layer->type() != LayerType::Text) {
        continue;
      }
      auto textLayer = reinterpret_cast<TextLayer*>(layer);
      if (textLayer->sourceText->animatable()) {
        auto keyframes =
            reinterpret_cast<AnimatableProperty<TextDocumentHandle>*>(textLayer->sourceText)
                ->keyframes;
        GetFontFromTextDocument(fontList, fontSet, keyframes[0]->startValue);
        for (auto& keyframe : keyframes) {
          GetFontFromTextDocument(fontList, fontSet, keyframe->endValue);
        }
      } else {
        GetFontFromTextDocument(fontList, fontSet, textLayer->sourceText->getValueAt(0));
      }
    }
  }
  return fontList;
}

static void WriteComposition(EncodeStream* stream, Composition* composition) {
  if (composition->type() == CompositionType::Vector) {
    WriteTag(stream, static_cast<VectorComposition*>(composition), WriteVectorComposition);
  } else if (composition->type() == CompositionType::Bitmap) {
    WriteTag(stream, static_cast<BitmapComposition*>(composition), WriteBitmapComposition);
  } else if (composition->type() == CompositionType::Video) {
    WriteTag(stream, static_cast<VideoComposition*>(composition), WriteVideoComposition);
  }
}

void WriteTagsOfFile(EncodeStream* stream, const File* file, PerformanceData* performanceData) {
  if (performanceData != nullptr) {
    WriteTag(stream, performanceData, WritePerformanceTag);
  }
  auto fileAttributes = file->fileAttributes;
  if (!fileAttributes.empty()) {
    WriteTag(stream, &fileAttributes, WriteFileAttributes);
  }
  if (file->timeStretchMode != PAGTimeStretchMode::Repeat || file->hasScaledTimeRange()) {
    WriteTag(stream, file, WriteTimeStretchMode);
  }
  auto fontList = GetFontList(file->compositions);
  if (!(fontList.empty())) {
    WriteTag(stream, &fontList, WriteFontTables);
  }
  if (!file->images.empty()) {
    WriteImages(stream, &(file->images));
  }
  if (file->editableImages != nullptr || file->editableTexts != nullptr) {
    WriteTag(stream, file, WriteEditableIndices);
  }
  auto func = std::bind(WriteComposition, stream, std::placeholders::_1);
  std::for_each(file->compositions.begin(), file->compositions.end(), func);
  WriteEndTag(stream);
}
}  // namespace pag
