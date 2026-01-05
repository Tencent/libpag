///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2023 Tencent. All rights reserved.
//
//  This library is free software; you can redistribute it and/or modify it under the terms of the
//  GNU Lesser General Public License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
//  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
//  the GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License along with this
//  library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
//  Boston, MA  02110-1301  USA
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace pag {
/**
 * This structure describes decoded (raw) video data.
 */
struct YUVBuffer {
  /**
   * The YUV data, each value is a pointer to the picture planes.
   */
  uint8_t* data[3];
  /**
   * The size in bytes of each picture line.
   */
  int lineSize[3];
};

/**
 * Codec specific data. For example: SPS (Sequence Parameter Sets) or PPS (Picture Parameter Sets).
 */
struct HeaderData {
  /**
   * The pointer of codec specific data
   */
  uint8_t* data;
  /**
   * The size in bytes of codec specific data.
   */
  size_t length;
};

/**
 * Possible results of calling SoftwareDecoder's methods.
 */
enum class DecoderResult {
  /**
   * The calling is successful.
   */
  Success = 0,
  /**
   * Output is not available in this state, need more input buffers.
   */
  TryAgainLater = -1,
  /**
   * The calling fails.
   */
  Error = -2
};

/**
 * Base class for interacting with software decoder created externally to PAG.
 */
class SoftwareDecoder {
 public:
  virtual ~SoftwareDecoder() = default;

  /**
   * Configure the software decoder.
   * @param headers Codec specific data. for example: {0 : sps, 1: pps}
   * @param mimeType MIME type. for example: "video/avc"
   * @param width video width
   * @param height video height
   * @return Return true if configure successfully.
   */
  virtual bool onConfigure(const std::vector<HeaderData>& headers, std::string mimeType, int width,
                           int height) = 0;

  /**
   * Send a sample data of encoded video frame for decoding.
   * @param bytes: The sample data for decoding.
   * @param length: The size of sample data
   * @param timestamp: The timestamp of this sample data.
   */
  virtual DecoderResult onSendBytes(void* bytes, size_t length, int64_t timestamp) = 0;

  /**
   * Try to decode a new frame from the pending frames sent by onSendBytes(). More input buffers
   * need to be sent by onSendBytes() if SoftwareDecodeResult::TryAgainLater was returned.
   */
  virtual DecoderResult onDecodeFrame() = 0;

  /**
   * Called to notify there is no more sample data available.
   */
  virtual DecoderResult onEndOfStream() = 0;

  /**
   * Called when seeking happens to clear all pending frames.
   */
  virtual void onFlush() = 0;

  /**
   * Returns decoded video data to render.
   */
  virtual std::unique_ptr<YUVBuffer> onRenderFrame() = 0;
};

/**
 * The factory of software decoder.
 */
class SoftwareDecoderFactory {
 public:
  virtual ~SoftwareDecoderFactory() = default;

  /**
   * Create a software decoder
   */
  virtual std::unique_ptr<SoftwareDecoder> createSoftwareDecoder() = 0;
};
}  // namespace pag
