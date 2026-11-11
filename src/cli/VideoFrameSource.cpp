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

#include "cli/VideoFrameSource.h"
#include <algorithm>
#include <iostream>
#include "cli/CliUtils.h"

namespace pagx::cli {

std::shared_ptr<PAGXDocument> LoadVideoDocument(const std::string& inputFile,
                                                const std::vector<std::string>& fontFiles,
                                                const std::vector<std::string>& fallbacks,
                                                const std::string& command) {
  auto document = LoadDocument(inputFile, command);
  if (document == nullptr) {
    return nullptr;
  }
  if (document->hasUnresolvedImports()) {
    std::cerr << command << ": error: unresolved import directive, run 'pagx resolve' first\n";
    return nullptr;
  }
  FontConfig fontConfig = {};
  if (!LoadFontConfig(&fontConfig, fontFiles, fallbacks, command)) {
    return nullptr;
  }
  document->applyLayout(&fontConfig);
  return document;
}

std::unique_ptr<VideoFrameSource> VideoFrameSource::Make(std::shared_ptr<PAGXDocument> document,
                                                         const VideoExportOptions& options,
                                                         std::string* errorMsg) {
  auto exporter = VideoExporter::Make(document, options, errorMsg);
  if (exporter == nullptr) {
    return nullptr;
  }
  std::unique_ptr<VideoFrameSource> source(new VideoFrameSource());
  source->sourceDocument = std::move(document);
  source->frameExporter = std::move(exporter);
  return source;
}

const std::shared_ptr<PAGXDocument>& VideoFrameSource::document() const {
  return sourceDocument;
}

const std::shared_ptr<VideoExporter>& VideoFrameSource::exporter() const {
  return frameExporter;
}

bool RunVideoFrames(VideoFrameSource& source, const std::string& command,
                    const std::string& errorPrefix, VideoFrameConsumer consumer, void* context,
                    std::string* errorMsg) {
  const auto& exporter = source.exporter();
  auto frameCount = exporter->frameCount();
  int64_t progressStep = std::max<int64_t>(1, frameCount / 10);
  while (exporter->nextFrame()) {
    VideoFrameView frame = {exporter->currentFrame() - 1, exporter->pixels(), exporter->rowBytes()};
    if (!consumer(frame, context, errorMsg)) {
      std::cerr << errorPrefix << *errorMsg << "\n";
      return false;
    }
    if (frame.index % progressStep == 0) {
      std::cout << command << ": " << (frame.index * 100 / frameCount) << "% (" << frame.index
                << "/" << frameCount << ")" << std::endl;
    }
  }
  if (exporter->currentFrame() < frameCount) {
    std::cerr << errorPrefix << "failed to render frame " << exporter->currentFrame() << "\n";
    return false;
  }
  return true;
}

}  // namespace pagx::cli
