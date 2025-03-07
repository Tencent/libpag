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
#include "FileAttributes.h"
#include "StringUtil.h"
#include "RunScript.h"
#include "src/ui/qt/EnvConfig.h"
#include "AEUtils.h"

static int64_t GetTimeStamp() {
  time_t t = time(NULL);
  tm* lt = localtime(&t);
  int64_t ts = 0;
  ts = static_cast<int64_t>(lt->tm_year + 1900);
  ts = ts * 100 + lt->tm_mon + 1;
  ts = ts * 100 + lt->tm_mday;
  ts = ts * 100 + lt->tm_hour;
  ts = ts * 100 + lt->tm_min;
  ts = ts * 100 + lt->tm_sec;
  ts = ts * 1000 + 0;
  return ts;
}

void GetFileAttributes(pag::FileAttributes* fileAttributes, pagexporter::Context& context) {
  fileAttributes->timestamp = GetTimeStamp();
  fileAttributes->pluginVersion = GetPAGExporterVersion();
  fileAttributes->aeVersion = AEUtils::GetAeVersion();
  fileAttributes->systemVersion = GetSystemVersion();
  fileAttributes->author = GetAuthorName();

  if (context.scenes == ExportScenes::GeneralScene) {
    fileAttributes->scene = "General";
  } else if (context.scenes == ExportScenes::UIScene) {
    fileAttributes->scene = "UI";
  } else if (context.scenes == ExportScenes::VideoEditScene) {
    fileAttributes->scene = "VideoEdit";
  }

  for (auto warning : context.alertInfos.saveWarnings) {
    fileAttributes->warnings.push_back(warning.getMessage());
  }
}
