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

#include "AEResource.h"
#include <codecvt>
#include "AEUtils.h"
#include "CommonMethod.h"
#include "ConfigParam.h"
#include "src/cJSON/cJSON.h"
#include "src/exports/AEMarker/AEMarker.h"
#include "src/exports/Composition/Composition.h"
#include "src/ui/qt/EnvConfig.h"
static const std::string StorePathKey = "storePath";
static const std::string ExportFlagKey = "exportFlag";
static const std::vector<std::string> MarkerJsonKeys = {StorePathKey, ExportFlagKey};
static std::string GetAEResourceTypeName(AEResourceType type) {
  if (type == AEResourceType::Folder) {
    return "Folder";
  } else if (type == AEResourceType::Composition) {
    return "Composition";
  } else if (type == AEResourceType::Image) {
    return "Image";
  } else {
    return "Unknown";
  }
}

static AEResourceType GetResourceType(const AEGP_SuiteHandler& suites, AEGP_ItemH& item) {
  AEGP_ItemType itemType = AEGP_ItemType_NONE;
  suites.ItemSuite8()->AEGP_GetItemType(item, &itemType);
  if (itemType == AEGP_ItemType_FOLDER) {
    return AEResourceType::Folder;
  }
  if (itemType == AEGP_ItemType_COMP) {
    return AEResourceType::Composition;
  }
  if (itemType == AEGP_ItemType_FOOTAGE) {
    AEGP_ItemFlags itemFlags;
    suites.ItemSuite6()->AEGP_GetItemFlags(item, &itemFlags);
    if (itemFlags & AEGP_ItemFlag_STILL) {
      AEGP_FootageH footageHandle;
      suites.FootageSuite5()->AEGP_GetMainFootageFromItem(item, &footageHandle);
      AEGP_FootageSignature signature;
      suites.FootageSuite5()->AEGP_GetFootageSignature(footageHandle, &signature);
      if (signature != AEGP_FootageSignature_SOLID && signature != AEGP_FootageSignature_MISSING &&
          signature != AEGP_FootageSignature_NONE) {
        return AEResourceType::Image;
      }
    }
  }
  return AEResourceType::Unknown;
}

static uint32_t GetParentFolderId(const AEGP_SuiteHandler& suites, AEGP_ItemH& item) {
  AEGP_ItemH parentItem = nullptr;
  suites.ItemSuite6()->AEGP_GetItemParentFolder(item, &parentItem);
  if (parentItem == nullptr) {
    return 0;
  }
  return AEUtils::GetItemId(parentItem);
}

static std::shared_ptr<AEResource> GetParentById(
    std::vector<std::shared_ptr<AEResource>>& resourceList, uint32_t id) {
  for (auto node : resourceList) {
    if (node->id == id) {
      return node;
    }
  }
  return nullptr;
}

static std::shared_ptr<AEResource> InstallResourceTree(
    std::vector<std::shared_ptr<AEResource>>& resourceList) {
  auto root = resourceList[0];
  if (root->id != 0) {
    root = AEResource::MakeAEResource();
    root->id = 0;
    root->type = AEResourceType::Folder;
    root->name = "root";
    resourceList.push_back(root);
  }
  for (auto node : resourceList) {
    if (node == root) {
      node->parent = nullptr;
    } else if (node->parent == nullptr) {
      node->parent = root;
      root->childs.push_back(node);
    } else {
      auto parent = GetParentById(resourceList, node->parent->id);
      //delete node->parent;
      node->parent = parent;
      parent->childs.push_back(node);
    }
  }
  return root;
}

static void PrintResourceList(std::vector<std::shared_ptr<AEResource>>& resourceList) {
  printf("PrintResourceList:\n");
  for (auto node : resourceList) {
    printf("%s id=%d parentId=%d type=%s path=%s\n", node->name.c_str(), node->id,
           node->parent ? node->parent->id : -1, GetAEResourceTypeName(node->type).c_str(),
           node->storePath.c_str());
  }
  printf("\n\n");
}

void AEResource::PrintResourceTree(std::shared_ptr<AEResource> node, int ident) {
  for (int i = 0; i < ident; i++) {
    printf("    ");
  }
  printf("%s (%s)\n", node->name.c_str(), GetAEResourceTypeName(node->type).c_str());
  for (auto child : node->childs) {
    PrintResourceTree(child, ident + 1);
  }
}

static std::string GetDefaultStorePath() {
  return AEUtils::GetProjectPath();
}

std::string AEResource::getStorePath() {
  if (type != AEResourceType::Composition) {
    return "";
  }
  auto defaultPath = GetDefaultStorePath();
  auto findPath = AEMarker::GetMarkerFromComposition(itemH, StorePathKey);
  if (findPath.empty()) {
    storePath = defaultPath;
  } else if (!AEUtils::IsExistPath(QStringToString(findPath))) {
    storePath = defaultPath;
    AEMarker::DeleteMarkersFromComposition(itemH, StorePathKey);
  } else {
    storePath = findPath;
  }
  return storePath;
}

bool AEResource::getExportFlag() {
  if (type != AEResourceType::Composition) {
    return false;
  }
  auto value = AEMarker::GetMarkerFromComposition(itemH, ExportFlagKey);
  if (value == "1") {
    exportFlag = true;
  } else {
    exportFlag = false;
  }
  return exportFlag;
}

void AEResource::setExportFlag(bool flag) {
  if (type != AEResourceType::Composition) {
    return;
  }
  auto value = AEMarker::GetMarkerFromComposition(itemH, ExportFlagKey);
  if (flag) {
    if (value != "1") {
      if (!value.empty()) {
        AEMarker::DeleteMarkersFromComposition(itemH, ExportFlagKey);
      }
      auto node = AEMarker::KeyValueToJsonNode(ExportFlagKey, 1);
      AEMarker::AddMarkerToComposition(itemH, node);
    }
  } else {
    if (!value.empty()) {
      AEMarker::DeleteMarkersFromComposition(itemH, ExportFlagKey);
    }
  }
  exportFlag = flag;
}

static int RemoveEmptyFolder(std::shared_ptr<AEResource>& resource) {
  int sum = 0;
  auto& childs = resource->childs;
  for (int index = static_cast<int>(childs.size()) - 1; index >= 0; index--) {
    auto& child = childs[index];
    if (child->type == AEResourceType::Composition) {
      sum++;
    } else if (child->type == AEResourceType::Folder) {
      int num = RemoveEmptyFolder(child);
      if (num == 0) {
        childs.erase(childs.begin() + index);
      } else {
        sum += num;
      }
    }
  }
  return sum;
}

bool AEResource::HasCompositionResource() {
  auto& suites = SUITES();
  A_long numProjects = 0;
  suites.ProjSuite6()->AEGP_GetNumProjects(&numProjects);
  for (A_long i = 0; i < numProjects; i++) {
    AEGP_ProjectH projectHandle;
    suites.ProjSuite6()->AEGP_GetProjectByIndex(i, &projectHandle);

    AEGP_ItemH item;
    suites.ItemSuite6()->AEGP_GetFirstProjItem(projectHandle, &item);
    while (item != nullptr) {
      auto type = GetResourceType(suites, item);
      if (type == AEResourceType::Composition) {
        return true;
      }
      AEGP_ItemH nextItem;
      suites.ItemSuite6()->AEGP_GetNextProjItem(projectHandle, item, &nextItem);
      item = nextItem;
    }
  }
  return false;
}

std::shared_ptr<AEResource> AEResource::GetResourceTree() {
  auto& suites = SUITES();
  std::vector<std::shared_ptr<AEResource>> resourceList;
  //auto activeItem = AEUtils::GetActiveCompositionItem();
  A_long numProjects = 0;
  suites.ProjSuite6()->AEGP_GetNumProjects(&numProjects);
  for (A_long i = 0; i < numProjects; i++) {
    AEGP_ProjectH projectHandle;
    A_char projectsName[AEGP_MAX_PROJ_NAME_SIZE];
    suites.ProjSuite6()->AEGP_GetProjectByIndex(i, &projectHandle);
    suites.ProjSuite6()->AEGP_GetProjectName(projectHandle, projectsName);
    printf("projectsName=%s\n", projectsName);

    AEGP_ItemH item;
    suites.ItemSuite6()->AEGP_GetFirstProjItem(projectHandle, &item);
    while (item != nullptr) {
      auto type = GetResourceType(suites, item);
      if (type != AEResourceType::Unknown) {
        auto resource = MakeAEResource();
        resource->type = type;
        resource->name = AEUtils::GetItemName(item);
        resource->id = AEUtils::GetItemId(item);
        resource->itemH = item;
        resource->getStorePath();
        resource->getExportFlag();
        //resource->setExportFlagByActive(activeItem);
        resourceList.push_back(resource);

        auto parentId = GetParentFolderId(suites, item);
        if (parentId != 0) {
          auto parentFloder = MakeAEResource();
          parentFloder->id = parentId;
          resource->parent = parentFloder;
        }
      }

      AEGP_ItemH nextItem;
      suites.ItemSuite6()->AEGP_GetNextProjItem(projectHandle, item, &nextItem);
      //suites.ItemSuite6()->AEGP_DeleteItem(item);
      item = nextItem;
    }
  }
  if (resourceList.size() == 0) {
    return nullptr;
  }
  PrintResourceList(resourceList);
  auto root = InstallResourceTree(resourceList);
  RemoveEmptyFolder(root);
  return root;
}

std::shared_ptr<AEResource> AEResource::MakeAEResource() {
  AEResource* resource = new AEResource();
  return std::shared_ptr<AEResource>(resource);
}

void AEResource::select() {
  AEUtils::SelectItem(itemH);
}

void AEResource::setStorePath(std::string path) {
  if (type != AEResourceType::Composition) {
    return;
  }
  auto findPath = AEMarker::GetMarkerFromComposition(itemH, StorePathKey);
  auto defaultPath = GetDefaultStorePath();
  if (path.empty()) {
    if (!findPath.empty()) {
      AEMarker::DeleteMarkersFromComposition(itemH, StorePathKey);
    }
    path = defaultPath;
  } else if (!AEUtils::IsExistPath(QStringToString(path))) {
    return;
  }
  storePath = path;
  if (path == defaultPath) {
    if (!findPath.empty()) {
      AEMarker::DeleteMarkersFromComposition(itemH, StorePathKey);
    }
    return;
  }
  if (path == findPath) {
    return;
  }
  if (!findPath.empty()) {
    AEMarker::DeleteMarkersFromComposition(itemH, StorePathKey);
  }
  auto node = AEMarker::KeyValueToJsonNode(StorePathKey, path);
  AEMarker::AddMarkerToComposition(itemH, node);
}

bool AEResource::GetExportAudioFlag() {
  bool ret = false;
  auto projectFileName = AEUtils::GetProjectFileName();
  if (projectFileName.empty()) {
    return ret;
  }
  std::string configPath = GetConfigPath() + "StoreExportAudio.json";
  auto text = FileIO::ReadTextFile(configPath.c_str());
  auto cjson = cJSON_Parse(text.c_str());
  if (cjson != nullptr) {
    auto item = cJSON_GetObjectItem(cjson, projectFileName.c_str());
    if (item != nullptr && item->type == cJSON_Int) {
      ret = !!item->valueint;
    }
    cJSON_Delete(cjson);
  }
  return ret;
}

void AEResource::SetExportAudioFlag(bool flag) {
  auto projectFileName = AEUtils::GetProjectFileName();
  if (projectFileName.empty()) {
    return;
  }
  std::string configPath = GetConfigPath() + "StoreExportAudio.json";
  auto text = FileIO::ReadTextFile(configPath.c_str());
  auto cjson = cJSON_Parse(text.c_str());
  if (cjson == nullptr) {
    if (flag) {
      std::string newString = "{\"" + projectFileName + "\": 1 }";
      FileIO::WriteTextFile(configPath.c_str(), newString.c_str());
    }
    return;
  }
  bool needWrite = false;
  auto item = cJSON_GetObjectItem(cjson, projectFileName.c_str());
  if (item != nullptr && item->type == cJSON_Int) {
    if (!flag) {
      cJSON_DeleteItemFromObject(cjson, projectFileName.c_str());
      needWrite = true;
    } else if (item->valueint != 1) {
      item->valueint = 1;
      needWrite = true;
    }
  } else if (flag) {
    item = cJSON_CreateInt(1, -1);
    cJSON_AddItemToObject(cjson, projectFileName.c_str(), item);
    needWrite = true;
  }
  if (needWrite) {
    auto newText = cJSON_Print(cjson);
    FileIO::WriteTextFile(configPath.c_str(), newText);
  }
  cJSON_Delete(cjson);
}
