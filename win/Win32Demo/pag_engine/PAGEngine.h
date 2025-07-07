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
#include <windows.h>

// #ifdef PAGENGINE_EXPORTS
// #define PAGENGINE_API __declspec(dllexport)
// #else
// #define PAGENGINE_API __declspec(dllimport)
// #endif
#define PAGENGINE_API

typedef unsigned char byte;

namespace pagengine {

class PAGENGINE_API PAGEngineCallback {
 public:
  virtual void OnPagPlayEnd() = 0;
};

class PAGEngineInitData {
 public:
  HWND hwnd = nullptr;
  int width = 0;
  int height = 0;
  PAGEngineCallback* pag_engine_callback_ = nullptr;
};

class PAGFile {
 public:
  virtual ~PAGFile() = default;
  virtual unsigned int GetWidth() = 0;
  virtual unsigned int GetHeight() = 0;
  virtual unsigned int GetDuration() = 0;
};

struct PagSize {
  unsigned int width;
  unsigned int height;
  PagSize(unsigned int width, unsigned int height) : width(width), height(height) {}
};

class PAGENGINE_API PAGEngine {
 public:
  virtual ~PAGEngine() = default;
  virtual bool InitOnscreenRender(PAGEngineInitData init_data) = 0;

  // pag file
  virtual PagSize GetPagFileSize() = 0;
  virtual unsigned int GetPagFileDuration() = 0;
  virtual bool SetPagFile(const char* file_path, int path_length) = 0;
  virtual bool SetPagFileBuffer(const byte* file_buffer, int buffer_length) = 0;
  virtual bool IsPagFileValid() = 0;

  virtual bool SetProgress(double percent) = 0;
  virtual void SetLoop(bool loop) = 0;

  virtual bool Flush(bool animating) = 0;
  virtual bool Resize(int width, int height) = 0;
};

PAGENGINE_API PAGEngine* CreatePAGEngine();
PAGENGINE_API bool ReleasePAGEngine(PAGEngine* pag_engine);

}  // namespace pagengine
