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
#ifndef PROGRESSWINDOWIMP_H
#define PROGRESSWINDOWIMP_H
#include "ProgressWindow.h"
#include "ProgressBase.h"

class ProgressWindowImp final : public ProgressWindow {
public:
  ~ProgressWindowImp() override;
 explicit ProgressWindowImp(pagexporter::Context* context = nullptr, ProgressBase* progressBase = nullptr);
  void setTitleName(std::string& title) override;
  void setProgressTips(std::string& tips) override;

private:
  void setProgress(double progress) override;
  ProgressBase* handle = nullptr;
};

#endif //PROGRESSWINDOWIMP_H
