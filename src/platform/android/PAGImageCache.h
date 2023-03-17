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
namespace pag {

class PAGImageCache {
 public:
  static std::shared_ptr<PAGImageCache> Make(const std::string& path, int width, int height,
                                             int frameCount, bool needInit);
  ~PAGImageCache();
  bool savePixels(int frame, void* bitmapPixels, long byteCount);
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
  int frameCount = 0;
  int fd = -1;
};

}  // namespace pag
