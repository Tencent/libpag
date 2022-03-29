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

#include "SequenceReaderFactory.h"
#include "BitmapSequenceReader.h"

#ifdef PAG_BUILD_FOR_WEB
#include "platform/web/VideoSequenceReader.h"
#else
#include "VideoReader.h"
#include "VideoSequenceDemuxer.h"
#endif

namespace pag {
SequenceReaderFactory::SequenceReaderFactory(Sequence* sequence) : sequence(sequence) {
}

uint32_t SequenceReaderFactory::assetID() const {
  return sequence->composition->uniqueID;
}

bool SequenceReaderFactory::staticContent() const {
  return sequence->composition->staticContent();
}

bool SequenceReaderFactory::isVideo() const {
  return sequence->composition->type() == CompositionType::Video;
}

std::shared_ptr<SequenceReader> SequenceReaderFactory::makeReader(
    std::shared_ptr<File> file) const {
  if (sequence->composition->type() == CompositionType::Bitmap) {
    return std::make_shared<BitmapSequenceReader>(file, static_cast<BitmapSequence*>(sequence));
  }
  auto videoSequence = static_cast<VideoSequence*>(sequence);
#ifdef PAG_BUILD_FOR_WEB
  return std::make_shared<VideoSequenceReader>(file, videoSequence, policy);
#else
  auto demuxer = std::make_unique<VideoSequenceDemuxer>(file, videoSequence);
  return std::make_shared<VideoReader>(std::move(demuxer));
#endif
}
}  // namespace pag