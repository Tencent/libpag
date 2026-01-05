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

#include "BitmapCompositionTag.h"
#include <algorithm>
#include "BitmapSequence.h"
#include "CompositionAttributes.h"
#include "CompositionTag.h"
#include "LayerTag.h"

namespace pag {
void ReadTagsOfBitmapComposition(DecodeStream* stream, TagCode code,
                                 BitmapComposition* composition) {
  switch (code) {
    case TagCode::BitmapSequence: {
      auto sequence = ReadBitmapSequence(stream);
      sequence->composition = composition;
      composition->sequences.push_back(sequence);
    } break;
    default:
      ReadTagsOfComposition(stream, code, composition);
      break;
  }
}

BitmapComposition* ReadBitmapComposition(DecodeStream* stream) {
  auto composition = new BitmapComposition();
  composition->id = stream->readEncodedUint32();
  ReadTags(stream, composition, ReadTagsOfBitmapComposition);
  return composition;
}

static bool lessFirst(const BitmapSequence* item1, const BitmapSequence* item2) {
  return item1->width < item2->width;
}

TagCode WriteBitmapComposition(EncodeStream* stream, BitmapComposition* composition) {
  stream->writeEncodedUint32(composition->id);
  WriteTagsOfComposition(stream, composition);
  auto sequences = composition->sequences;
  std::sort(sequences.begin(), sequences.end(), lessFirst);
  for (auto sequence : sequences) {
    WriteTag(stream, sequence, WriteBitmapSequence);
  }
  WriteEndTag(stream);
  return TagCode::BitmapCompositionBlock;
}
}  // namespace pag
