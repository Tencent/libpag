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

#pragma once
#include "GPUDrawable.h"
#include "PAGEngine.h"
#include "pag/pag.h"

namespace pagengine {
class PAGENGINE_API PAGEngineImpl : public PAGEngine {
 public:
  PAGEngineImpl();
  virtual ~PAGEngineImpl();

  // PAGEngine
  bool InitOnscreenRender(PAGEngineInitData init_data) override;
  bool SetPagFile(const char* file_path, int path_length) override;
  bool SetPagFileBuffer(const byte* file_buffer, int buffer_length) override;
  bool IsPagFileValid() override;
  bool SetProgress(double percent) override;
  bool Flush(bool animating) override;
  bool Resize(int width, int height) override;
  PagSize GetPagFileSize() override;
  unsigned int GetPagFileDuration() override;
  void SetLoop(bool loop) override;

  bool DrawPAG(bool animating);

 private:
  void InitHighResolutionTime();
  int ElapseMsFromLastRenderTime(bool update = false);

 private:
  std::shared_ptr<pag::PAGSurface> pag_surface_;
  std::shared_ptr<pag::PAGFile> pag_file_;
  std::shared_ptr<pag::PAGPlayer> pag_player_;
  std::shared_ptr<pag::GPUDrawable> pag_drawable_;

  HWND render_window_ = nullptr;

  double current_progress_ = 0;

  LARGE_INTEGER last_render_time_;
  LARGE_INTEGER frequency_;

  PAGEngineCallback* pag_engine_callback_ = nullptr;
  bool need_premultiply_alpha_ = false;
  HWND hwnd_ = nullptr;
  int width_ = 0;
  int height_ = 0;
  bool is_loop_ = true;
};
}  // namespace pagengine
