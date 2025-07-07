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

#pragma once

#include "VideoDecoder.h"
#include "base/utils/Log.h"
#include "codec/NALUType.h"
#include "pag/decoder.h"
#include "pag/file.h"

namespace pag {
/**
 * The base class for factory of video decoder.
 */
class VideoDecoderFactory {
 public:
  /**
   * Returns the factory for creating the external video decoders. It always returns a non-nullptr
   * pointer. Use the HasExternalSoftwareDecoder() method to check whether there is a registered
   * external video decoder factory.
   */
  static const VideoDecoderFactory* ExternalDecoderFactory();

  /**
   * Returns the default video decoder factory for creating the SoftAVCDecoders. It always returns a
   * non-nullptr pointer.
   */
  static const VideoDecoderFactory* SoftwareAVCDecoderFactory();

  /**
   * Returns true if a external software decoder is registered. We assume that it has better
   * performance than libavc.
   */
  static bool HasExternalSoftwareDecoder();

  virtual ~VideoDecoderFactory() = default;

  /**
   * Creates a video decoder.
   */
  std::unique_ptr<VideoDecoder> createDecoder(const VideoFormat& format) const;

  /**
   * Returns true if the factory creates hardware video decoders.
   */
  virtual bool isHardwareBacked() const = 0;

 protected:
  virtual std::unique_ptr<VideoDecoder> onCreateDecoder(const VideoFormat& format) const = 0;

 private:
  static void NotifyHardwareVideoDecoderReleased();

  friend class VideoDecoder;
};
}  // namespace pag
