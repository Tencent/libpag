/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#ifndef PROGRESSWINDOW_H
#define PROGRESSWINDOW_H
#include "src/ui/qt/Progress/ProgressBase.h"
#include "src/exports/PAGDataTypes.h"

class ProgressWindow {
public:
  static std::unique_ptr<ProgressWindow> MakeProgressWindow(pagexporter::Context* context = nullptr,
                                                            ProgressBase* progressBase = nullptr);

  virtual ~ProgressWindow() = default;

  void setTotalFrames(float totalFrames);
  void addTotalFrames(float addFrames);
  void increaseEncodedFrames();
  void addEncodedFrames(float addFrames);
  void addEncodedFramesExtra(float addFrames = 1.0f);  // addFrames通常小于等于1.0f
  void resetEncodedFrames();
  virtual void setTitleName(std::string& title) = 0;
  virtual void setProgressTips(std::string& tips) = 0;

private:
  virtual void setProgress(double progress) = 0;

  float totalFrames = 1.0f;
  float encodedFrames = 0.0f;
};
#endif //PROGRESSWINDOW_H
