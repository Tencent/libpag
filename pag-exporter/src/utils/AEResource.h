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

#include <QtCore/QMetaType>
#include <memory>  // windows for std::shared_ptr
#include <string>
#include <vector>
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"

#ifndef AERESOURCE_H
#define AERESOURCE_H
enum class AEResourceType { Unknown, Folder, Composition, Image };

class AEResource {
 public:
  // 供资源面板调用，获取资源树
  static std::shared_ptr<AEResource> GetResourceTree();
  static bool GetExportAudioFlag();
  static void SetExportAudioFlag(bool flag);
  static bool HasCompositionResource();

  static void PrintResourceTree(std::shared_ptr<AEResource> node, int ident = 0);

  static std::shared_ptr<AEResource> MakeAEResource();

  AEResource() {
  }

  ~AEResource() {
  }

  void select();
  std::string getStorePath();
  void setStorePath(std::string path);
  bool getExportFlag();
  void setExportFlag(bool flag);

  AEResourceType type = AEResourceType::Unknown;
  uint32_t id = 0;
  std::string name = "";
  AEGP_ItemH itemH = nullptr;
  std::string storePath = "";
  bool exportFlag = false;
  std::shared_ptr<AEResource> parent = nullptr;
  std::vector<std::shared_ptr<AEResource>> childs;
};

Q_DECLARE_METATYPE(AEResource)
Q_DECLARE_METATYPE(std::shared_ptr<AEResource>)

#endif  //AERESOURCE_H
