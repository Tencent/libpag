/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions and
//  limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "pagx/runtime/SequencePlayer.h"
#include <cmath>
#include "pag/file.h"
#include "pagx/nodes/Image.h"
#include "pagx/pag/SequenceCodec.h"
#include "rendering/sequences/BitmapSequenceReader.h"
#include "rendering/sequences/SequenceInfo.h"
#include "rendering/sequences/SequenceReader.h"
#include "rendering/sequences/VideoReader.h"
#include "rendering/sequences/VideoSequenceDemuxer.h"

namespace pagx {

// Owns one rebuilt legacy sequence subtree. The legacy structures use raw pointers and manual
// deletes, so a shared owner keeps them alive exactly as long as the reader needs them.
// The legacy structures own their children (VideoFrame deletes fileBytes, sequences delete
// frames and headers, compositions delete layers), so releasing the two roots frees the whole
// rebuilt tree. The holder must not delete any child pointers itself.
struct LegacySequenceHolder {
  pag::Composition* composition = nullptr;
  pag::Sequence* sequence = nullptr;

  ~LegacySequenceHolder() {
    delete sequence;
    delete composition;
  }
};

// Rebuilds a pag::VideoSequence from the decoded container. The frames' payloads are copied
// into ByteData blocks owned by the holder; the composition is an empty stand-in whose
// staticContent() reports false (empty static time ranges), which routes the reader onto the
// dynamic decoding path.
std::shared_ptr<SequencePlayer> BuildVideoPlayer(
    const VideoSequenceContainer& container, const std::shared_ptr<LegacySequenceHolder>& holder) {
  auto* composition = new pag::VideoComposition();
  auto* sequence = new pag::VideoSequence();
  sequence->composition = composition;
  sequence->width = static_cast<int32_t>(container.width);
  sequence->height = static_cast<int32_t>(container.height);
  sequence->frameRate = container.frameRate;
  sequence->alphaStartX = container.alphaStartX;
  sequence->alphaStartY = container.alphaStartY;
  for (auto& header : container.headers) {
    sequence->headers.push_back(pag::ByteData::MakeCopy(header->bytes(), header->size()).release());
  }
  for (auto& frame : container.frames) {
    auto* legacyFrame = new pag::VideoFrame();
    legacyFrame->isKeyframe = frame.isKeyframe;
    legacyFrame->frame = static_cast<pag::Frame>(frame.frameIndex);
    if (frame.data != nullptr) {
      legacyFrame->fileBytes =
          pag::ByteData::MakeCopy(frame.data->bytes(), frame.data->size()).release();
    }
    sequence->frames.push_back(legacyFrame);
  }
  holder->composition = composition;
  holder->sequence = sequence;
  // The demuxer only keeps the file reference alive; the holder owns the rebuilt sequence, so
  // a null file reference is safe here.
  auto demuxer = std::make_unique<pag::VideoSequenceDemuxer>(nullptr, sequence);
  auto player = std::shared_ptr<SequencePlayer>(new SequencePlayer());
  // The default platform factory list (hardware first, SoftAVCDecoder fallback) — exactly the
  // decoder configuration the legacy runtime uses. The strict soft-decode comparison gate of
  // P3-15 forces this list down to software when it runs.
  player->reader = std::make_shared<pag::VideoReader>(std::move(demuxer));
  player->holder = holder;
  player->_frameRate = container.frameRate;
  player->_frameCount = static_cast<uint32_t>(container.frames.size());
  player->_width = static_cast<int>(container.width);
  player->_height = static_cast<int>(container.height);
  player->_isVideo = true;
  player->_alphaStartX = container.alphaStartX;
  player->_alphaStartY = container.alphaStartY;
  return player;
}

// Rebuilds a pag::BitmapSequence the same way.
std::shared_ptr<SequencePlayer> BuildBitmapPlayer(
    const BitmapSequenceContainer& container, const std::shared_ptr<LegacySequenceHolder>& holder) {
  auto* composition = new pag::BitmapComposition();
  auto* sequence = new pag::BitmapSequence();
  sequence->composition = composition;
  sequence->width = static_cast<int32_t>(container.width);
  sequence->height = static_cast<int32_t>(container.height);
  sequence->frameRate = container.frameRate;
  for (auto& frame : container.frames) {
    auto* legacyFrame = new pag::BitmapFrame();
    legacyFrame->isKeyframe = frame.isKeyframe;
    for (auto& block : frame.bitmaps) {
      auto* rect = new pag::BitmapRect();
      rect->x = block.x;
      rect->y = block.y;
      if (block.data != nullptr) {
        rect->fileBytes =
            pag::ByteData::MakeCopy(block.data->bytes(), block.data->size()).release();
      }
      legacyFrame->bitmaps.push_back(rect);
    }
    sequence->frames.push_back(legacyFrame);
  }
  holder->composition = composition;
  holder->sequence = sequence;
  auto player = std::shared_ptr<SequencePlayer>(new SequencePlayer());
  player->reader = std::make_shared<pag::BitmapSequenceReader>(nullptr, sequence);
  player->holder = holder;
  player->_frameRate = container.frameRate;
  player->_frameCount = static_cast<uint32_t>(container.frames.size());
  player->_width = static_cast<int>(container.width);
  player->_height = static_cast<int>(container.height);
  return player;
}

std::shared_ptr<SequencePlayer> SequencePlayer::Make(const Image* sequenceImage) {
  if (sequenceImage == nullptr || !sequenceImage->isSequence || sequenceImage->data == nullptr ||
      sequenceImage->data->empty()) {
    return nullptr;
  }
  auto holder = std::make_shared<LegacySequenceHolder>();
  if (sequenceImage->sequenceMediaType == 2) {
    VideoSequenceContainer container = {};
    if (!SequenceCodec::DecodeVideoSequence(sequenceImage->data, &container)) {
      return nullptr;
    }
    return BuildVideoPlayer(container, holder);
  }
  if (sequenceImage->sequenceMediaType == 3) {
    BitmapSequenceContainer container = {};
    if (!SequenceCodec::DecodeBitmapSequence(sequenceImage->data, &container)) {
      return nullptr;
    }
    return BuildBitmapPlayer(container, holder);
  }
  return nullptr;
}

std::shared_ptr<tgfx::Image> SequencePlayer::frameAt(double compositionTime) {
  if (reader == nullptr || _frameCount == 0 || _frameRate <= 0.0f) {
    return nullptr;
  }
  // Legacy TimeToFrame rounding: floor(time * frameRate), clamped to the sequence range.
  auto seconds = compositionTime < 0.0 ? 0.0 : compositionTime;
  auto rawFrame = static_cast<int64_t>(floor(seconds * static_cast<double>(_frameRate)));
  auto targetFrame =
      static_cast<pag::Frame>(std::min<int64_t>(rawFrame, static_cast<int64_t>(_frameCount) - 1));
  auto generator = std::make_shared<pag::SequenceFrameGenerator>(reader, targetFrame, nullptr);
  auto image = tgfx::Image::MakeFrom(std::move(generator));
  if (image != nullptr && _isVideo) {
    // Video frames carry a dual-region buffer (RGB + alpha areas, legacy RGBAAA layout);
    // resample into the display size exactly like SequenceInfo::MakeSequenceImage does.
    image = image->makeRGBAAA(_width, _height, _alphaStartX, _alphaStartY);
  }
  return image;
}

float SequencePlayer::frameRate() const {
  return _frameRate;
}

uint32_t SequencePlayer::frameCount() const {
  return _frameCount;
}

int SequencePlayer::width() const {
  return _width;
}

int SequencePlayer::height() const {
  return _height;
}
}  // namespace pagx
