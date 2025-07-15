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

#include <atomic>
#include "pag/file.h"
#include "pag/pag.h"
#include "rendering/Performance.h"
#include "tgfx/core/ImageBuffer.h"

namespace pag {
class SequenceReader {
 public:
  virtual ~SequenceReader() = default;

  /**
   * Returns the width of the sequence buffers created from the reader.
   */
  virtual int width() const = 0;

  /**
   * Returns the height of the sequence buffers created from the reader.
   */
  virtual int height() const = 0;

  /**
   * Decodes the specified target frame immediately and returns the decoded image buffer.
   */
  std::shared_ptr<tgfx::ImageBuffer> readBuffer(Frame targetFrame);

  void reportPerformance(Performance* performance);

 protected:
  /**
   * Return the decoded ImageBuffer of the specified frame.
   */
  virtual std::shared_ptr<tgfx::ImageBuffer> onMakeBuffer(Frame targetFrame) = 0;

  /**
   * Reports the decoding performance data.
   */
  virtual void onReportPerformance(Performance* performance, int64_t decodingTime) = 0;

 private:
  std::atomic_int64_t decodingTime = 0;
};
}  // namespace pag
