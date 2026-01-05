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

#include "AudioBytes.h"
#include "CompositionAttributes.h"
#include "MarkerTag.h"

namespace pag {
void ReadTagsOfComposition(DecodeStream* stream, TagCode code, Composition* composition) {
  switch (code) {
    case TagCode::CompositionAttributes:
      ReadCompositionAttributes(stream, composition);
      break;
    case TagCode::AudioBytes:
      ReadAudioBytes(stream, composition);
      break;
    case TagCode::MarkerList:
      ReadMarkerList(stream, &composition->audioMarkers);
      break;
    default:
      break;
  }
}

void WriteTagsOfComposition(EncodeStream* stream, Composition* composition) {
  WriteTag(stream, composition, WriteCompositionAttributes);
  if (composition->audioBytes != nullptr) {
    WriteTag(stream, composition, WriteAudioBytes);
    if (!composition->audioMarkers.empty()) {
      WriteTag(stream, &composition->audioMarkers, WriteMarkerList);
    }
  }
}
}  // namespace pag
