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

#include "AEHelper.h"
#include <QDir>
#include <QFileInfo>
#include <fstream>
#include <iostream>
#include <thread>
#include "AEDataTypeConverter.h"
#include "ImageData.h"
#include "StringHelper.h"
#include "TempFileDelete.h"
#include "platform/PlatformHelper.h"
#include "src/base/utils/Log.h"
#include "ui/WindowManager.h"

namespace exporter {

AEGP_PluginID PluginID = 0L;
std::shared_ptr<AEGP_SuiteHandler> Suites = nullptr;
std::string DocumentsFolderPath = "";
std::string AeVersion = "";

int32_t MAJORVERSION = 23;

// Static member definitions
int32_t AEVersion::MajorVerison = 0;
int32_t AEVersion::MinorVersion = 0;

std::string GetDocumentsFolderPath() {
  if (DocumentsFolderPath.empty()) {
    DocumentsFolderPath = RunScript("Folder.myDocuments.fsName;");
  }
  return DocumentsFolderPath;
}

std::string GetAEVersion() {
  if (AeVersion.empty()) {
    AeVersion = RunScript("app.version");
  }
  return AeVersion;
}

std::shared_ptr<AEGP_SuiteHandler> GetSuites() {
  return Suites;
}

AEGP_PluginID GetPluginID() {
  return PluginID;
}

QString GetProjectName() {
  AEGP_ProjectH projectHandle = nullptr;
  Suites->ProjSuite6()->AEGP_GetProjectByIndex(0, &projectHandle);
  AEGP_MemHandle pathMemory;
  Suites->ProjSuite6()->AEGP_GetProjectPath(projectHandle, &pathMemory);
  std::string filePath = AeMemoryHandleToString(pathMemory);
  Suites->MemorySuite1()->AEGP_FreeMemHandle(pathMemory);
  if (!filePath.empty()) {
    std::replace(filePath.begin(), filePath.end(), '\\', '/');
  }
  QString projectPath =
      QDir::cleanPath(QDir::fromNativeSeparators(QString::fromStdString(filePath)));
  QFileInfo fileInfo(projectPath);
  return fileInfo.fileName();
}

QString GetProjectPath() {
  AEGP_ProjectH projectHandle = nullptr;
  Suites->ProjSuite6()->AEGP_GetProjectByIndex(0, &projectHandle);
  AEGP_MemHandle pathMemory;
  Suites->ProjSuite6()->AEGP_GetProjectPath(projectHandle, &pathMemory);
  std::string filePath = AeMemoryHandleToString(pathMemory);
  Suites->MemorySuite1()->AEGP_FreeMemHandle(pathMemory);
  if (!filePath.empty()) {
    std::replace(filePath.begin(), filePath.end(), '\\', '/');
  }
  QString projectPath =
      QDir::cleanPath(QDir::fromNativeSeparators(QString::fromStdString(filePath)));
  QFileInfo fileInfo(projectPath);
  return fileInfo.absolutePath();
}

AEGP_ItemH GetActiveCompositionItem() {
  AEGP_ItemH activeItemHandle = nullptr;
  Suites->ItemSuite6()->AEGP_GetActiveItem(&activeItemHandle);
  if (activeItemHandle == nullptr) {
    AEGP_ProjectH projectHandle = nullptr;
    Suites->ProjSuite6()->AEGP_GetProjectByIndex(0, &projectHandle);
    Suites->ItemSuite6()->AEGP_GetFirstProjItem(projectHandle, &activeItemHandle);
  }
  if (activeItemHandle == nullptr) {
    return nullptr;
  }
  AEGP_ItemType itemType = AEGP_ItemType_NONE;
  Suites->ItemSuite6()->AEGP_GetItemType(activeItemHandle, &itemType);
  if (itemType != AEGP_ItemType_COMP) {
    return nullptr;
  }
  return activeItemHandle;
}

void GetRenderFrame(uint8* rgbaBytes, A_u_long srcStride, A_u_long dstStride, A_long width,
                    A_long height, AEGP_RenderOptionsH& renderOptions) {
  if (rgbaBytes == nullptr || width <= 0 || height <= 0 || srcStride <= 0 || dstStride <= 0) {
    return;
  }
  Suites->RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);
  Suites->RenderOptionsSuite3()->AEGP_SetDownsampleFactor(renderOptions, 1, 1);

  AEGP_FrameReceiptH frameReceipt;
  Suites->RenderSuite5()->AEGP_RenderAndCheckoutFrame(renderOptions, nullptr, nullptr,
                                                      &frameReceipt);

  AEGP_WorldH imageWorld;
  Suites->RenderSuite5()->AEGP_GetReceiptWorld(frameReceipt, &imageWorld);

  PF_Pixel* pixels = nullptr;
  Suites->WorldSuite3()->AEGP_GetBaseAddr8(imageWorld, &pixels);

  ConvertARGBToRGBA(&(pixels->alpha), width, height, srcStride, rgbaBytes, dstStride);
  Suites->RenderSuite5()->AEGP_CheckinFrame(frameReceipt);
}

void GetRenderFrameSize(AEGP_RenderOptionsH& renderOptions, A_u_long& stride, A_long& width,
                        A_long& height) {
  Suites->RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);
  Suites->RenderOptionsSuite3()->AEGP_SetDownsampleFactor(renderOptions, 1, 1);

  AEGP_FrameReceiptH frameReceipt;
  Suites->RenderSuite5()->AEGP_RenderAndCheckoutFrame(renderOptions, nullptr, nullptr,
                                                      &frameReceipt);
  AEGP_WorldH imageWorld;
  Suites->RenderSuite5()->AEGP_GetReceiptWorld(frameReceipt, &imageWorld);
  Suites->WorldSuite3()->AEGP_GetSize(imageWorld, &width, &height);
  Suites->WorldSuite3()->AEGP_GetRowBytes(imageWorld, &stride);

  Suites->RenderSuite5()->AEGP_CheckinFrame(frameReceipt);
}

void GetLayerRenderFrame(uint8* rgbaBytes, A_u_long srcStride, A_u_long dstStride, A_long width,
                         A_long height, AEGP_LayerRenderOptionsH& renderOptions) {
  if (rgbaBytes == nullptr || width <= 0 || height <= 0 || srcStride <= 0 || dstStride <= 0) {
    return;
  }
  Suites->LayerRenderOptionsSuite2()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);
  Suites->LayerRenderOptionsSuite2()->AEGP_SetDownsampleFactor(renderOptions, 1, 1);

  AEGP_FrameReceiptH frameReceipt;
  Suites->RenderSuite5()->AEGP_RenderAndCheckoutLayerFrame(renderOptions, nullptr, nullptr,
                                                           &frameReceipt);

  AEGP_WorldH imageWorld;
  Suites->RenderSuite5()->AEGP_GetReceiptWorld(frameReceipt, &imageWorld);

  PF_Pixel* pixels = nullptr;
  Suites->WorldSuite3()->AEGP_GetBaseAddr8(imageWorld, &pixels);

  exporter::ConvertARGBToRGBA(&(pixels->alpha), width, height, srcStride, rgbaBytes, dstStride);
  Suites->RenderSuite5()->AEGP_CheckinFrame(frameReceipt);
}

void GetLayerRenderFrameSize(AEGP_LayerRenderOptionsH& renderOptions, A_u_long& stride,
                             A_long& width, A_long& height) {
  Suites->LayerRenderOptionsSuite2()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);
  Suites->LayerRenderOptionsSuite2()->AEGP_SetDownsampleFactor(renderOptions, 1, 1);

  AEGP_FrameReceiptH frameReceipt;
  Suites->RenderSuite5()->AEGP_RenderAndCheckoutLayerFrame(renderOptions, nullptr, nullptr,
                                                           &frameReceipt);
  AEGP_WorldH imageWorld;
  Suites->RenderSuite5()->AEGP_GetReceiptWorld(frameReceipt, &imageWorld);
  Suites->WorldSuite3()->AEGP_GetSize(imageWorld, &width, &height);
  Suites->WorldSuite3()->AEGP_GetRowBytes(imageWorld, &stride);

  Suites->RenderSuite5()->AEGP_CheckinFrame(frameReceipt);
}

std::vector<char> GetProjectFileBytes() {
  std::vector<char> fileBytes = {};
  AEGP_ProjectH projectHandle = nullptr;
  Suites->ProjSuite6()->AEGP_GetProjectByIndex(0, &projectHandle);

  AEGP_MemHandle pathMemory;
  Suites->ProjSuite6()->AEGP_GetProjectPath(projectHandle, &pathMemory);
  char16_t* projectPath = nullptr;
  Suites->MemorySuite1()->AEGP_LockMemHandle(pathMemory, reinterpret_cast<void**>(&projectPath));

  std::string filePath = Utf16ToUtf8(projectPath);
  Suites->MemorySuite1()->AEGP_FreeMemHandle(pathMemory);

  A_Boolean isDirty = 0;
  Suites->ProjSuite6()->AEGP_ProjectIsDirty(projectHandle, &isDirty);
  bool isAEPX = false;

  if (!filePath.empty()) {
    auto extension = filePath.substr(filePath.size() - 5, 5);
    isAEPX = ToLowerCase(extension) == ".aepx";
  }

  exporter::TempFileDelete tempFile;
  if (isDirty || filePath.empty() || isAEPX) {
    auto tempFolder = exporter::GetTempFolderPath();
    if (!tempFolder.empty() && tempFolder.back() == '/') {
      tempFolder.pop_back();
    }
    filePath = tempFolder + u8"/.PAGAutoSave.aep";
    tempFile.setFilePath(filePath);
    auto path = Utf8ToUtf16(filePath);
    auto saveResult = Suites->ProjSuite6()->AEGP_SaveProjectToPath(
        projectHandle, reinterpret_cast<const A_UTF16Char*>(path.c_str()));
    if (saveResult != 0) {
      return fileBytes;
    }

    // AEGP_SaveProjectToPath can complete asynchronously, and there might be a delay
    // before the file is fully written to disk by the file system.
    // Therefore, we need a retry loop to wait for the file to become accessible
    // before attempting to read it.
    int maxRetries = 50;
    int retryCount = 0;
    std::ifstream testFile(filePath, std::ios::binary);

    // Loop to check if the file has been created and can be opened.
    while (!testFile.is_open() && retryCount < maxRetries) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      retryCount++;
      testFile.open(filePath, std::ios::binary);
    }
    testFile.close();

    if (retryCount >= maxRetries) {
      auto errorMsg = QObject::tr(
          "Failed to save project file. The file could not be written to disk after multiple "
          "attempts. Please check disk space and file permissions, then try again.");
      WindowManager::GetInstance().showSimpleError(errorMsg);
      return fileBytes;
    }
  }

  if (filePath.empty()) {
    return fileBytes;
  }

  filePath = ConvertStringEncoding(filePath);

  std::ifstream t(filePath, std::ios::binary);
  if (!t.is_open()) {
    return fileBytes;
  }

  t.seekg(0, std::ios::end);
  auto fileLength = t.tellg();
  t.seekg(0, std::ios::beg);

  if (fileLength == 0) {
    t.close();
    return fileBytes;
  }

  fileBytes.resize(fileLength);
  t.read(fileBytes.data(), fileLength);
  t.close();

  return fileBytes;
}

void SetRenderTime(const AEGP_RenderOptionsH& renderOptions, float frameRate, pag::Frame frame) {
  A_Time time = {};
  time.value = static_cast<A_long>(1000 * frame);
  time.scale = static_cast<A_u_long>(std::lround(1000 * frameRate));
  Suites->RenderOptionsSuite3()->AEGP_SetTime(renderOptions, time);

  A_Time currentTime = {};
  Suites->RenderOptionsSuite3()->AEGP_GetTime(renderOptions, &currentTime);
  if (currentTime.value != time.value || currentTime.scale != time.scale) {
    LOGE("GetCompositionFrameImage: GetTime failed.");
  }
}

void SetSuitesAndPluginID(SPBasicSuite* basicSuite, AEGP_PluginID id) {
  Suites = std::make_shared<AEGP_SuiteHandler>(basicSuite);
  PluginID = id;
}

std::string RunScript(const std::string& scriptText) {
  AEGP_MemHandle scriptResult;
  AEGP_MemHandle errorResult;
  Suites->UtilitySuite6()->AEGP_ExecuteScript(PluginID, scriptText.c_str(), FALSE, &scriptResult,
                                              &errorResult);
  A_char* result = nullptr;
  Suites->MemorySuite1()->AEGP_LockMemHandle(scriptResult, reinterpret_cast<void**>(&result));
  std::string resultText = result;
  Suites->MemorySuite1()->AEGP_FreeMemHandle(scriptResult);
  A_char* error = nullptr;
  Suites->MemorySuite1()->AEGP_LockMemHandle(errorResult, reinterpret_cast<void**>(&error));
  std::string errorText = error;
  Suites->MemorySuite1()->AEGP_FreeMemHandle(errorResult);
  return resultText;
}

void RunScriptPreWarm() {
  static bool hasInit = false;
  if (!hasInit) {
    RegisterTextDocumentScript();
    GetDocumentsFolderPath();
    GetAEVersion();
    hasInit = true;
  }
}

bool CheckAeVersion() {
  int32_t majorVersion = 0;
  if (AeVersion.empty() || AEVersion::MajorVerison == 0) {
    return false;
  }
  try {
    size_t dotPos = AeVersion.find('.');
    if (dotPos == std::string::npos) {
      LOGE("Invalid version format!");
      return false;
    }
    std::string versionStr = AeVersion.substr(0, dotPos);
    majorVersion = std::stoi(versionStr);
  } catch (const std::invalid_argument& e) {
    LOGE("Invalid argument: %s", e.what());
    return false;
  } catch (const std::out_of_range& e) {
    LOGE("Out of range: %s", e.what());
    return false;
  }
  if (majorVersion >= MAJORVERSION) {
    return true;
  }
  return false;
}

void SetMajorVersion(int32_t majorVersion) {
  AEVersion::MajorVerison = majorVersion;
}

void setMinorVersion(int32_t minorVersion) {
  AEVersion::MinorVersion = minorVersion;
}

void RegisterTextDocumentScript() {
  static bool hasInit = false;
  if (!hasInit) {
    std::string textDocumentScript = GetJavaScriptFromQRC(":/scripts/GetTextDocument.js");
    RunScript(textDocumentScript);
    hasInit = true;
  }
}

uint32_t GetLayerID(const AEGP_LayerH& layerHandle) {
  A_long id = 0;
  if (layerHandle != nullptr) {
    Suites->LayerSuite6()->AEGP_GetLayerID(layerHandle, &id);
  }
  return static_cast<uint32_t>(id);
}

uint32_t GetLayerItemID(const AEGP_LayerH& layerHandle) {
  auto itemHandle = GetLayerItemH(layerHandle);
  auto id = GetItemID(itemHandle);
  return id;
}

std::string GetLayerName(const AEGP_LayerH& layerHandle) {
  std::string layerName;
  if (layerHandle == nullptr) {
    return layerName;
  }
  AEGP_MemHandle layerNameHandle = nullptr;
  AEGP_MemHandle sourceNameHandle = nullptr;
  Suites->LayerSuite6()->AEGP_GetLayerName(PluginID, layerHandle, &layerNameHandle,
                                           &sourceNameHandle);
  if (!layerNameHandle || !sourceNameHandle) {
    return layerName;
  }
  layerName = AeMemoryHandleToString(layerNameHandle);
  if (layerName.empty()) {
    layerName = AeMemoryHandleToString(sourceNameHandle);
  }
  Suites->MemorySuite1()->AEGP_FreeMemHandle(layerNameHandle);
  Suites->MemorySuite1()->AEGP_FreeMemHandle(sourceNameHandle);

  layerName = DeleteLastSpace(layerName);
  return layerName;
}

AEGP_ItemH GetLayerItemH(const AEGP_LayerH& layerHandle) {
  AEGP_ItemH itemHandle = nullptr;
  Suites->LayerSuite6()->AEGP_GetLayerSourceItem(layerHandle, &itemHandle);
  return itemHandle;
}

pag::Ratio GetLayerStretch(const AEGP_LayerH& layerHandle) {
  A_Ratio ratio = {};
  Suites->LayerSuite6()->AEGP_GetLayerStretch(layerHandle, &ratio);
  return {ratio.num, ratio.den};
}

pag::Frame GetLayerStartTime(const AEGP_LayerH& layerHandle, float frameRate) {
  A_Time inPoint = {};
  Suites->LayerSuite6()->AEGP_GetLayerInPoint(layerHandle, AEGP_LTimeMode_CompTime, &inPoint);
  if (inPoint.scale == 0) {
    return 0;
  }
  return static_cast<pag::Frame>(std::round(inPoint.value * frameRate / inPoint.scale));
}

pag::Frame GetLayerDuration(const AEGP_LayerH& layerHandle, float frameRate) {
  A_Time duration = {};
  Suites->LayerSuite6()->AEGP_GetLayerDuration(layerHandle, AEGP_LTimeMode_CompTime, &duration);
  auto frames = static_cast<pag::Frame>(std::round(duration.value * frameRate / duration.scale));
  return std::abs(frames);
}

AEGP_LayerFlags GetLayerFlags(const AEGP_LayerH& layerHandle) {
  AEGP_LayerFlags flags;
  Suites->LayerSuite6()->AEGP_GetLayerFlags(layerHandle, &flags);
  return flags;
}

AEGP_LayerH GetLayerParentLayerH(const AEGP_LayerH& layerHandle) {
  AEGP_LayerH parentLayerHandle = nullptr;
  Suites->LayerSuite6()->AEGP_GetLayerParent(layerHandle, &parentLayerHandle);
  return parentLayerHandle;
}

pag::BlendMode GetLayerBlendMode(const AEGP_LayerH& layerHandle) {
  AEGP_LayerTransferMode transferMode = {};
  Suites->LayerSuite6()->AEGP_GetLayerTransferMode(layerHandle, &transferMode);
  return AEXferToBlendMode(transferMode.mode);
}

AEGP_LayerH GetLayerTrackMatteLayerH(const AEGP_LayerH& layerHandle) {
  AEGP_LayerH trackMatteLayerHandle = nullptr;
  Suites->LayerSuite9()->AEGP_GetTrackMatteLayer(layerHandle, &trackMatteLayerHandle);
  return trackMatteLayerHandle;
}

pag::TrackMatteType GetLayerTrackMatteType(const AEGP_LayerH& layerHandle) {
  A_Boolean hasTrackMatte = false;
  Suites->LayerSuite6()->AEGP_DoesLayerHaveTrackMatte(layerHandle, &hasTrackMatte);
  if (!hasTrackMatte) {
    return pag::TrackMatteType::None;
  }

  AEGP_LayerTransferMode transferMode = {};
  Suites->LayerSuite6()->AEGP_GetLayerTransferMode(layerHandle, &transferMode);
  return AETrackMatteToTrackMatteType(transferMode.track_matte);
}

A_long GetLayerEffectNum(const AEGP_LayerH& layerHandle) {
  AEGP_LayerFlags layerFlags;
  Suites->LayerSuite6()->AEGP_GetLayerFlags(layerHandle, &layerFlags);
  if ((layerFlags & AEGP_LayerFlag_EFFECTS_ACTIVE) == 0) {
    return 0;
  }
  A_long numEffects = 0;
  Suites->EffectSuite4()->AEGP_GetLayerNumEffects(layerHandle, &numEffects);
  return numEffects;
}

void SelectLayer(const AEGP_ItemH& itemHandle, const AEGP_LayerH& layerHandle) {
  if (itemHandle == nullptr) {
    return;
  }
  bool hasLayer = (layerHandle != nullptr);
  AEGP_CollectionItemV2 collectionItem;
  AEGP_StreamRefH streamHandle = nullptr;
  if (hasLayer) {
    Suites->DynamicStreamSuite4()->AEGP_GetNewStreamRefForLayer(PluginID, layerHandle,
                                                                &streamHandle);
    collectionItem.type = AEGP_CollectionItemType_LAYER;
    collectionItem.u.layer.layerH = layerHandle;
    collectionItem.stream_refH = streamHandle;
  }

  Suites->ItemSuite6()->AEGP_SelectItem(itemHandle, true, true);
  if (!hasLayer) {
    /* 3061: Open selection, ignoring any modifier keys. */
    Suites->CommandSuite1()->AEGP_DoCommand(3061);
    return;
  }

  auto compositionHandle = GetItemCompH(itemHandle);
  AEGP_Collection2H collectionHandle = nullptr;
  Suites->CollectionSuite2()->AEGP_NewCollection(PluginID, &collectionHandle);
  Suites->CollectionSuite2()->AEGP_CollectionPushBack(collectionHandle, &collectionItem);
  Suites->CompSuite6()->AEGP_SetSelection(compositionHandle, collectionHandle);
  /* 3061: Open selection, ignoring any modifier keys. */
  Suites->CommandSuite1()->AEGP_DoCommand(3061);
  Suites->CollectionSuite2()->AEGP_DisposeCollection(collectionHandle);
}

uint32_t GetItemID(const AEGP_ItemH& itemHandle) {
  A_long id = 0;
  if (itemHandle != nullptr) {
    Suites->ItemSuite6()->AEGP_GetItemID(itemHandle, &id);
  }
  return static_cast<uint32_t>(id);
}

uint32_t GetItemParentID(const AEGP_ItemH& itemHandle) {
  uint32_t id = 0;
  AEGP_ItemH parentItemHandle = nullptr;
  Suites->ItemSuite6()->AEGP_GetItemParentFolder(itemHandle, &parentItemHandle);
  if (parentItemHandle != nullptr) {
    id = GetItemID(parentItemHandle);
  }
  return id;
}

std::string GetItemName(const AEGP_ItemH& itemHandle) {
  std::string itemName;
  if (itemHandle == nullptr) {
    return itemName;
  }
  AEGP_MemHandle memoryHandle = nullptr;
  Suites->ItemSuite8()->AEGP_GetItemName(PluginID, itemHandle, &memoryHandle);
  if (!memoryHandle) {
    return itemName;
  }
  itemName = AeMemoryHandleToString(memoryHandle);
  Suites->MemorySuite1()->AEGP_FreeMemHandle(memoryHandle);

  itemName = DeleteLastSpace(itemName);
  return itemName;
}

AEGP_CompH GetItemCompH(const AEGP_ItemH& itemHandle) {
  AEGP_CompH compositionHandle = nullptr;
  Suites->CompSuite6()->AEGP_GetCompFromItem(itemHandle, &compositionHandle);
  return compositionHandle;
}

float GetItemFrameRate(const AEGP_ItemH& itemHandle) {
  auto compositionHandle = GetItemCompH(itemHandle);
  A_FpLong frameRate = 0;
  Suites->CompSuite6()->AEGP_GetCompFramerate(compositionHandle, &frameRate);
  return static_cast<float>(frameRate);
}

pag::Frame GetItemDuration(const AEGP_ItemH& itemHandle) {
  A_Time time = {};
  Suites->ItemSuite6()->AEGP_GetItemDuration(itemHandle, &time);
  A_FpLong frameRate = GetItemFrameRate(itemHandle);
  return static_cast<pag::Frame>(std::round(time.value * frameRate / time.scale));
}

QSize GetItemDimensions(const AEGP_ItemH& itemHandle) {
  A_long width = 0;
  A_long height = 0;
  Suites->ItemSuite8()->AEGP_GetItemDimensions(itemHandle, &width, &height);
  return {width, height};
}

QImage GetCompositionFrameImage(const AEGP_ItemH& itemHandle, pag::Frame frame) {
  if (itemHandle == nullptr) {
    return {};
  }
  AEGP_RenderOptionsH renderOptions = nullptr;
  float frameRate = GetItemFrameRate(itemHandle);

  Suites->RenderOptionsSuite3()->AEGP_NewFromItem(PluginID, itemHandle, &renderOptions);
  if (renderOptions == nullptr) {
    return {};
  }
  Suites->RenderOptionsSuite3()->AEGP_SetWorldType(renderOptions, AEGP_WorldType_8);
  SetRenderTime(renderOptions, frameRate, frame);

  uint8_t* rgbaBytes = nullptr;
  A_u_long stride = 0;
  A_long width = 0;
  A_long height = 0;
  GetRenderFrameSize(renderOptions, stride, width, height);
  if (width <= 0 || height <= 0 || stride <= 0) {
    return {};
  }
  rgbaBytes = new uint8_t[stride * height];
  GetRenderFrame(rgbaBytes, stride, stride, width, height, renderOptions);
  QImage image(rgbaBytes, width, height, stride, QImage::Format_RGBA8888);
  QImage saveImage = image.copy();
  delete[] rgbaBytes;
  return saveImage;
}

void SetItemName(const AEGP_ItemH& itemHandle, const std::string& name) {
  std::u16string u16str = Utf8ToUtf16(name);
  Suites->ItemSuite8()->AEGP_SetItemName(itemHandle,
                                         reinterpret_cast<const A_UTF16Char*>(u16str.data()));
}

void SelectItem(const AEGP_ItemH& itemHandle) {
  if (itemHandle != nullptr) {
    Suites->ItemSuite6()->AEGP_SelectItem(itemHandle, true, true);
    /* 3061: Open selection, ignoring any modifier keys. */
    Suites->CommandSuite1()->AEGP_DoCommand(3061);
  }
}

std::string GetCompName(const AEGP_CompH& compositionHandle) {
  auto itemHandle = GetCompItemH(compositionHandle);
  return GetItemName(itemHandle);
}

AEGP_ItemH GetCompItemH(const AEGP_CompH& compositionHandle) {
  AEGP_ItemH itemHandle = nullptr;
  Suites->CompSuite6()->AEGP_GetItemFromComp(compositionHandle, &itemHandle);
  return itemHandle;
}

pag::Color GetCompBackgroundColor(const AEGP_CompH& compositionHandle) {
  AEGP_ColorVal color;
  Suites->CompSuite6()->AEGP_GetCompBGColor(compositionHandle, &color);
  return AEColorToColor(color);
}

bool IsStaticComposition(const AEGP_CompH& compositionHandle) {
  bool isStatic = true;
  AEGP_ItemH itemHandle = GetCompItemH(compositionHandle);
  pag::Frame totalFrames = GetItemDuration(itemHandle);
  if (totalFrames <= 1) {
    return isStatic;
  }

  AEGP_RenderOptionsH renderOptions = nullptr;
  float frameRate = GetItemFrameRate(itemHandle);

  Suites->RenderOptionsSuite3()->AEGP_NewFromItem(PluginID, itemHandle, &renderOptions);
  if (renderOptions == nullptr) {
    return isStatic;
  }

  uint8_t* curData = nullptr;
  uint8_t* preData = nullptr;
  pag::Frame step = 1;
  while (true) {
    pag::Frame value = totalFrames / step;
    if (value < 100) {
      break;
    }
    step *= 10;
  }
  for (pag::Frame frame = 0; frame < totalFrames; frame += step) {
    SetRenderTime(renderOptions, frameRate, frame);
    A_long width = 0;
    A_long height = 0;
    A_u_long stride = 0;
    GetRenderFrameSize(renderOptions, stride, width, height);
    if (curData == nullptr) {
      curData = new uint8_t[stride * height];
    }
    GetRenderFrame(curData, stride, stride, width, height, renderOptions);
    if (curData != nullptr && preData != nullptr &&
        !exporter::ImageIsStatic(curData, preData, width, height, stride)) {
      isStatic = false;
      break;
    }
    std::swap(curData, preData);
  }

  if (curData != nullptr) {
    delete[] curData;
  }
  if (preData != nullptr) {
    delete[] preData;
  }
  Suites->RenderOptionsSuite3()->AEGP_Dispose(renderOptions);

  return isStatic;
}

std::string GetStreamMatchName(const AEGP_StreamRefH& streamHandle) {
  char matchName[200] = {0};
  Suites->DynamicStreamSuite4()->AEGP_GetMatchName(streamHandle, matchName);
  return matchName;
}

bool IsStreamHidden(const AEGP_StreamRefH& streamHandle) {
  AEGP_DynStreamFlags flags;
  Suites->DynamicStreamSuite4()->AEGP_GetDynamicStreamFlags(streamHandle, &flags);
  return static_cast<bool>(flags & AEGP_DynStreamFlag_HIDDEN);
}

bool IsStreamActive(const AEGP_StreamRefH& streamHandle) {
  AEGP_DynStreamFlags flags;
  Suites->DynamicStreamSuite4()->AEGP_GetDynamicStreamFlags(streamHandle, &flags);
  return (flags & AEGP_DynStreamFlag_ACTIVE_EYEBALL) > 0;
}

AEGP_StreamRefH GetLayerMarkerStream(const AEGP_LayerH& layerHandle) {
  const auto& suites = GetSuites();
  auto pluginID = GetPluginID();
  AEGP_StreamRefH streamHandle = nullptr;
  suites->StreamSuite4()->AEGP_GetNewLayerStream(pluginID, layerHandle, AEGP_LayerStream_MARKER,
                                                 &streamHandle);
  return streamHandle;
}

AEGP_StreamRefH GetItemMarkerStream(const AEGP_ItemH& itemHandle) {
  auto compositionHandle = GetItemCompH(itemHandle);
  return GetCompositionMarkerStream(compositionHandle);
}

AEGP_StreamRefH GetCompositionMarkerStream(const AEGP_CompH& compositionHandle) {
  if (compositionHandle == nullptr) {
    return nullptr;
  }
  const auto& suites = GetSuites();
  auto pluginID = GetPluginID();
  AEGP_StreamRefH streamRefHandle = nullptr;
  suites->CompSuite10()->AEGP_GetNewCompMarkerStream(pluginID, compositionHandle, &streamRefHandle);
  return streamRefHandle;
}

void DeleteStream(AEGP_StreamRefH streamHandle) {
  if (streamHandle != nullptr) {
    const auto& suites = GetSuites();
    suites->StreamSuite4()->AEGP_DisposeStream(streamHandle);
    streamHandle = nullptr;
  }
}

}  // namespace exporter
