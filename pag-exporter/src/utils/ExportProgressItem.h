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
#ifndef EXPORTPROGRESSITEM_H
#define EXPORTPROGRESSITEM_H
#include <string>
#include <vector>
#include "src/utils/AEResource.h"

static const int DEFAULT_ITEM_HEIGHT = 40;
static const int STATUS_WAITING = 0;
static const int STATUS_SUCCESS = 1;
static const int STATUS_WRONG = 2;

class ExportProgressItem {
public:
  ExportProgressItem(std::string pagName);

  std::string pagName = "";
  std::string errorInfo = "";
  int statusCode = STATUS_WAITING;
  int itemHeight = DEFAULT_ITEM_HEIGHT;
  int textHeight = 0;
  float progressValue = 0;
  std::shared_ptr<AEResource> aeResource = nullptr;
  std::string saveFilePath = "";
  bool isExportAudio = false;

  void updateErrorTextInfo(std::string newErrorInfo);
};
#endif //EXPORTPROGRESSITEM_H
