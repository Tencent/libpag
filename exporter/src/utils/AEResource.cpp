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

#include "AEResource.h"
#include <QStandardPaths>
#include <unordered_map>
#include "AEHelper.h"
#include "StringHelper.h"
#include "export/Marker.h"

namespace exporter {

AEResourceType GetAEItemResourceType(AEGP_ItemH& item) {
  const auto& suites = GetSuites();

  AEGP_ItemType itemType = AEGP_ItemType_NONE;
  suites->ItemSuite8()->AEGP_GetItemType(item, &itemType);
  if (itemType == AEGP_ItemType_FOLDER) {
    return AEResourceType::Folder;
  }
  if (itemType == AEGP_ItemType_COMP) {
    return AEResourceType::Composition;
  }
  if (itemType == AEGP_ItemType_FOOTAGE) {
    AEGP_ItemFlags itemFlags;
    suites->ItemSuite6()->AEGP_GetItemFlags(item, &itemFlags);
    if (itemFlags & AEGP_ItemFlag_STILL) {
      AEGP_FootageH footageHandle = nullptr;
      suites->FootageSuite5()->AEGP_GetMainFootageFromItem(item, &footageHandle);
      AEGP_FootageSignature signature;
      suites->FootageSuite5()->AEGP_GetFootageSignature(footageHandle, &signature);
      if (signature != AEGP_FootageSignature_SOLID && signature != AEGP_FootageSignature_MISSING &&
          signature != AEGP_FootageSignature_NONE) {
        return AEResourceType::Image;
      }
    }
  }
  return AEResourceType::Unknown;
}

bool HasCompositionResource() {
  const auto& suites = GetSuites();
  A_long numProjects = 0;
  suites->ProjSuite6()->AEGP_GetNumProjects(&numProjects);
  for (A_long i = 0; i < numProjects; i++) {
    AEGP_ProjectH projectHandle = nullptr;
    suites->ProjSuite6()->AEGP_GetProjectByIndex(i, &projectHandle);

    AEGP_ItemH item;
    suites->ItemSuite6()->AEGP_GetFirstProjItem(projectHandle, &item);
    while (item != nullptr) {
      auto type = GetAEItemResourceType(item);
      if (type == AEResourceType::Composition) {
        return true;
      }
      AEGP_ItemH nextItem;
      suites->ItemSuite6()->AEGP_GetNextProjItem(projectHandle, item, &nextItem);
      item = nextItem;
    }
  }
  return false;
}

std::vector<std::shared_ptr<AEResource>> AEResource::GetAEResourceList() {
  std::vector<std::shared_ptr<AEResource>> resources = {};
  std::unordered_map<A_long, std::shared_ptr<AEResource>> resourceMap = {};

  const auto& suites = GetSuites();
  A_long projectsNum = 0;
  suites->ProjSuite6()->AEGP_GetNumProjects(&projectsNum);
  for (A_long index = 0; index < projectsNum; index++) {
    AEGP_ProjectH projectHandle = nullptr;
    suites->ProjSuite6()->AEGP_GetProjectByIndex(index, &projectHandle);
    A_char projectName[AEGP_MAX_PROJ_NAME_SIZE] = {0};
    suites->ProjSuite6()->AEGP_GetProjectName(projectHandle, projectName);
    AEGP_ItemH itemHandle = nullptr;
    suites->ItemSuite6()->AEGP_GetFirstProjItem(projectHandle, &itemHandle);
    while (itemHandle != nullptr) {
      A_long id = GetItemID(itemHandle);
      if (id != 0) {
        auto resource = std::make_shared<AEResource>();
        auto type = GetAEItemResourceType(itemHandle);
        if (type != AEResourceType::Unknown) {
          resource->type = type;
          resource->ID = id;
          resource->name = GetItemName(itemHandle);
          resource->itemHandle = itemHandle;
          resource->isExportAsBmp = IsEndWidthSuffix(resource->name, CompositionBmpSuffix);
          resources.push_back(resource);
          resourceMap[id] = resource;
          auto parentID = GetItemParentID(itemHandle);
          auto parentIter = resourceMap.find(parentID);
          if (parentIter != resourceMap.end()) {
            parentIter->second->file.children.push_back(resource);
            resource->file.parent = parentIter->second.get();
          }
        }
      }

      AEGP_ItemH nextItemHandle = nullptr;
      suites->ItemSuite6()->AEGP_GetNextProjItem(projectHandle, itemHandle, &nextItemHandle);
      itemHandle = nextItemHandle;
    }
  }

  auto iter = resources.begin();
  while (iter != resources.end()) {
    if ((*iter)->type != AEResourceType::Folder && (*iter)->type != AEResourceType::Composition) {
      iter = resources.erase(iter);
      continue;
    }
    if ((*iter)->type == AEResourceType::Folder && (*iter)->file.children.empty()) {
      auto parent = (*iter)->file.parent;
      if (parent != nullptr) {
        parent->file.children.erase(
            std::find(parent->file.children.begin(), parent->file.children.end(), *iter));
      }
      iter = resources.erase(iter);
      continue;
    }

    ++iter;
  }

  return resources;
}

AEResource::AEResource() {
  initSavePath();
}

void AEResource::setSavePath(const std::string& savePath) {
  this->savePath = savePath;
  SetCompositionStoragePath(savePath, itemHandle);
}

void AEResource::initSavePath() {
  std::string path = GetCompositionStoragePath(itemHandle);
  if (path.empty()) {
    path = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation).toStdString();
  }
  savePath = path;
}

}  // namespace exporter
