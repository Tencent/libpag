/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <memory>
#include <mutex>
namespace pag {

class ImageCache {
 public:
  static std::shared_ptr<ImageCache> Make(const std::string& path, int width, int height,
                                          int frameCount, bool needInit);
  ~ImageCache();
  bool flushSave();
  bool putPixelsToSaveBuffer(int frame, void* bitmapPixels, int byteCount);
  bool inflatePixels(int frame, void* bitmapPixels, int byteCount);
  bool isCached(int frame);
  bool isAllCached();
  void releaseSaveBuffer();
  void release();

 private:
  void* compressBuffer = nullptr;
  void* deCompressBuffer = nullptr;
  int deCompressBufferSize = 0;
  void* headerBuffer = nullptr;
  void* pendingSaveBuffer = nullptr;
  int pendingSaveBufferSize = 0;
  int pendingSaveFrame = -1;
  int frameCount = 0;
  int fd = -1;
};

}  // namespace pag
