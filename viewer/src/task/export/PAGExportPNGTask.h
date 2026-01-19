/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "task/PAGTask.h"

namespace pag {

class PAGExportPNGTask : public PAGPlayTask {
  Q_OBJECT
 public:
  // use to export a single frame of a pag file as png file.
  explicit PAGExportPNGTask(std::shared_ptr<PAGFile> pagFile, const QString& pngfilePath,
                            int exportFrame);
  // use to export a pag file as png sequcence.
  explicit PAGExportPNGTask(std::shared_ptr<PAGFile> pagFile, const QString& pngSequenceDirPath);

 protected:
  void onFrameFlush(double progress) override;
  bool isNeedRenderCurrentFrame() override;

 private:
  void exportCurrentFrameAsPNG(const QString& outPath);

 private:
  int exportFrame = -1;
};

}  // namespace pag
