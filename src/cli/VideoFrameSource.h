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
//  unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "as is" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "pagx/VideoExporter.h"

namespace pagx::cli {

struct VideoFrameView {
  int64_t index = 0;
  const uint8_t* pixels = nullptr;
  size_t rowBytes = 0;
};

using VideoFrameConsumer = bool (*)(const VideoFrameView& frame, void* context,
                                    std::string* errorMsg);

/** Loads a document and applies the supplied fonts before it is used for video frame rendering. */
std::shared_ptr<PAGXDocument> LoadVideoDocument(const std::string& inputFile,
                                                const std::vector<std::string>& fontFiles,
                                                const std::vector<std::string>& fallbacks,
                                                const std::string& command);

/** Owns the document and exporter shared by the MP4 and image-sequence command paths. */
class VideoFrameSource {
 public:
  /** Creates a frame source or returns nullptr with a human-readable error message. */
  static std::unique_ptr<VideoFrameSource> Make(std::shared_ptr<PAGXDocument> document,
                                                const VideoExportOptions& options,
                                                std::string* errorMsg);

  /** Returns the source document retained by this frame source. */
  const std::shared_ptr<PAGXDocument>& document() const;

  /** Returns the underlying pull-based frame exporter. */
  const std::shared_ptr<VideoExporter>& exporter() const;

 private:
  std::shared_ptr<PAGXDocument> sourceDocument = {};
  std::shared_ptr<VideoExporter> frameExporter = {};
};

/**
 * Renders every frame through consumer and reports progress using command-specific prefixes.
 */
bool RunVideoFrames(VideoFrameSource& source, const std::string& command,
                    const std::string& errorPrefix, VideoFrameConsumer consumer, void* context,
                    std::string* errorMsg);

}  // namespace pagx::cli
