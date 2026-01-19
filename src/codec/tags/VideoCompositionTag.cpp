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

#include "VideoCompositionTag.h"
#include <algorithm>
#include "CompositionAttributes.h"
#include "CompositionTag.h"
#include "LayerTag.h"
#include "VideoSequence.h"

namespace pag {
void ReadTagsOfVideoComposition(DecodeStream* stream, TagCode code,
                                std::pair<VideoComposition*, bool>* parameter) {
  auto composition = parameter->first;
  auto hasAlpha = parameter->second;
  switch (code) {
    case TagCode::VideoSequence: {
      auto sequence = ReadVideoSequence(stream, hasAlpha);
      sequence->composition = composition;
      composition->sequences.push_back(sequence);
    } break;
    case TagCode::Mp4Header: {
      for (auto sequence : composition->sequences) {
        if (sequence->MP4Header == nullptr) {
          sequence->MP4Header = ReadMp4Header(stream);
          break;
        }
      }
    } break;
    default:
      ReadTagsOfComposition(stream, code, composition);
      break;
  }
}

VideoComposition* ReadVideoComposition(DecodeStream* stream) {
  auto composition = new VideoComposition();
  composition->id = stream->readEncodedUint32();
  auto hasAlpha = stream->readBoolean();
  auto parameter = std::make_pair(composition, hasAlpha);
  ReadTags(stream, &parameter, ReadTagsOfVideoComposition);
  return composition;
}

static bool lessFirst(const VideoSequence* item1, const VideoSequence* item2) {
  return item1->width < item2->width;
}

TagCode WriteVideoComposition(EncodeStream* stream, VideoComposition* composition) {
  auto sequences = composition->sequences;
  std::sort(sequences.begin(), sequences.end(), lessFirst);
  auto hasAlpha =
      !sequences.empty() && (sequences[0]->alphaStartY > 0 || sequences[0]->alphaStartX > 0);
  stream->writeEncodedUint32(composition->id);
  stream->writeBoolean(hasAlpha);
  WriteTagsOfComposition(stream, composition);
  for (auto sequence : sequences) {
    auto parameter = std::make_pair(sequence, hasAlpha);
    WriteTag(stream, &parameter, WriteVideoSequence);
  }
  for (auto sequence : sequences) {
    if (sequence->MP4Header == nullptr) {
      break;
    }
    WriteTag(stream, sequence->MP4Header, WriteMp4Header);
  }
  WriteEndTag(stream);
  return TagCode::VideoCompositionBlock;
}
}  // namespace pag
