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
#include "ProgressWindowImp.h"
#include "CustomProgressBar.h"

ProgressWindowImp::~ProgressWindowImp() {
  delete handle;
  handle = nullptr;
}

ProgressWindowImp::ProgressWindowImp(pagexporter::Context* context, ProgressBase* progressBase) {
  if (progressBase == nullptr) {
    handle = new CustomProgressBar(context);
  } else {
    handle = progressBase;
  }
  handle->context = context;
  setProgress(0.0);
}

void ProgressWindowImp::setProgress(double progress) {
  handle->setProgress(progress);
}

void ProgressWindowImp::setTitleName(std::string& title) {
  const QString titleName = QString::fromUtf8(title.c_str());
  handle->setProgressTitle(titleName);
}

void ProgressWindowImp::setProgressTips(std::string& tips) {
  const QString tipsContent = QString::fromUtf8(tips.c_str());
  handle->setProgressTips(tipsContent);
}
