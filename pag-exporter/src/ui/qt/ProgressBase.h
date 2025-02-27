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
#ifndef PROGRESSBASE_H
#define PROGRESSBASE_H
#include <QtCore/QString>
#include "Context.h"

class ProgressBase {
 public:
  ProgressBase() {};
  virtual ~ProgressBase() = default;

  virtual void setProgress(double progressValue) = 0;
  virtual void setProgressTips(const QString& tip) = 0;
  virtual void setProgressTitle(const QString& title) = 0;

  pagexporter::Context* context = nullptr;
};
#endif  //PROGRESSBASE_H
