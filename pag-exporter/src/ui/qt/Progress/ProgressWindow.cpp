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
#include "ProgressWindow.h"
#include "ProgressWindowImp.h"
std::unique_ptr<ProgressWindow> ProgressWindow::MakeProgressWindow(pagexporter::Context* context,
                                                                   ProgressBase* progressBase) {
  auto progressWindow = new ProgressWindowImp(context, progressBase);
  return std::unique_ptr<ProgressWindow>(progressWindow);
}

void ProgressWindow::setTotalFrames(float totalFrames) {
  this->totalFrames = totalFrames;
  encodedFrames = 0.0f;
}

void ProgressWindow::addTotalFrames(float addFrames) {
  totalFrames += addFrames;
}

void ProgressWindow::increaseEncodedFrames() {
  encodedFrames += 1.0f;
  if (totalFrames > 0.0f) {
    setProgress(100.0 * encodedFrames / totalFrames);
  }
}

void ProgressWindow::addEncodedFrames(float addFrames) {
  encodedFrames += addFrames;
  if (totalFrames > 0.0f) {
    setProgress(100.0 * encodedFrames / totalFrames);
  }
}

void ProgressWindow::addEncodedFramesExtra(float addFrames) {
  totalFrames += addFrames;
  addEncodedFrames(addFrames);
}

void ProgressWindow::resetEncodedFrames() {
  encodedFrames = 0.0f;
  setProgress(0.0f);
}