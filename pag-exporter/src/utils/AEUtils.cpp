#include "AEUtils.h"
#ifdef WIN32
#include <io.h>  // windows for access
#endif
#include <QtWidgets/QFileDialog>
#include <qdebug.h>
#include <codecvt>
#include <iostream>
#include "RunScript.h"
#include "StringUtil.h"

AEGP_PluginID AEUtils::PluginID = 0L;
AEGP_SuiteHandler* AEUtils::Suites = nullptr;
std::string AEUtils::LastOutputPath;
std::string AEUtils::LastFilePath;
std::string AEUtils::SequenceSuffix = "_bmp";
std::string AEUtils::AeVersion;
std::string AEUtils::FolderDocumentsName;
std::string AEUtils::FolderTempName;

void AEUtils::RegisterTextDocumentScript() {
  static bool hasInit = false;
  if (!hasInit) {
    auto& suites = SUITES();
    auto pluginID = PLUGIN_ID();
    RunScript(suites, pluginID, TextDocumentScript);
    hasInit = true;
  }
}

void AEUtils::SetSuitesAndPluginID(SPBasicSuite* basicSuite, AEGP_PluginID id) {
  Suites = new AEGP_SuiteHandler(basicSuite);
  PluginID = id;
}

void AEUtils::GetLanguageString(char language[]) {
  auto& suites = SUITES();
  suites.AppSuite5()->PF_AppGetLanguage(language);
}

void AEUtils::RunScriptPreWarm() {
  static bool hasInit = false;
  if (!hasInit) {
    RegisterTextDocumentScript();
    GetFolderDocumentsName();
    GetFolderTempName();
    GetAeVersion();
    hasInit = true;
  }
}

bool AEUtils::RunOpenConfigPanelScript() {
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  auto configPanelCommand = RunScript(suites, pluginID, "app.findMenuCommandId(\"PAG Config\")");
  if (configPanelCommand != "0") {
    RunScript(suites, pluginID, "app.executeCommand(" + configPanelCommand + ")");
    return true;
  }
  return false;
}

std::string AEUtils::GetAeVersion() {
  if (AeVersion.empty()) {
    auto& suites = SUITES();
    auto pluginID = PLUGIN_ID();
    AeVersion = RunScript(suites, pluginID, "app.version");
  }
  return AeVersion;
}

std::string AEUtils::GetFolderDocumentsName() {
  if (FolderDocumentsName.empty()) {
    auto& suites = SUITES();
    auto pluginID = PLUGIN_ID();
    FolderDocumentsName = RunScript(suites, pluginID, "Folder.myDocuments.fsName;");
  }
  return FolderDocumentsName;
}

std::string AEUtils::GetFolderTempName() {
  if (FolderTempName.empty()) {

#ifdef WIN32
    auto& suites = SUITES();
    auto pluginID = PLUGIN_ID();
    FolderTempName = RunScript(suites, pluginID, "Folder.myDocuments.fsName;");
#elif defined(__APPLE__) || defined(__MACH__)
    // macOS下Folder.temp.fsName;获取到的系统临时目录AE没有写入权限
    // 因此将临时文件保存至系统文件夹/tmp，该文件夹系统会定期删除文件
    FolderTempName = "/tmp";
#endif
  }
  return FolderTempName;
}

AEGP_ItemH AEUtils::GetActiveCompositionItem() {
  auto& suites = SUITES();
  AEGP_ItemH activeItemH = nullptr;
  suites.ItemSuite6()->AEGP_GetActiveItem(&activeItemH);
  if (activeItemH == nullptr) {
    AEGP_ProjectH projectHandle;
    suites.ProjSuite6()->AEGP_GetProjectByIndex(0, &projectHandle);
    suites.ItemSuite6()->AEGP_GetFirstProjItem(projectHandle, &activeItemH);
  }
  if (activeItemH == nullptr) {
    return nullptr;
  }
  AEGP_ItemType itemType = AEGP_ItemType_NONE;
  suites.ItemSuite6()->AEGP_GetItemType(activeItemH, &itemType);
  if (itemType != AEGP_ItemType_COMP) {
    return nullptr;
  }
  return activeItemH;
}

std::string AEUtils::GetProjectFileName() {
  auto& suites = SUITES();
  AEGP_ProjectH projectHandle;
  suites.ProjSuite6()->AEGP_GetProjectByIndex(0, &projectHandle);
  AEGP_MemHandle pathMemory;
  suites.ProjSuite6()->AEGP_GetProjectPath(projectHandle, &pathMemory);
  std::string filePath = StringUtil::ToString(suites, pathMemory);
  suites.MemorySuite1()->AEGP_FreeMemHandle(pathMemory);
  if (!filePath.empty()) {
    std::replace(filePath.begin(), filePath.end(), '\\', '/');
  }
  return filePath;
}

std::string AEUtils::GetProjectPath() {
  auto filePath = GetProjectFileName();
  if (filePath.empty()) {
    filePath = AEUtils::GetFolderDocumentsName() + "/";
  }
  std::replace(filePath.begin(), filePath.end(), '\\', '/');
  auto index = filePath.rfind('/');
  if (index != std::string::npos) {
    filePath = filePath.substr(0, index + 1);
  }
  return filePath;
}

static void InsureStringSuffix(std::string& filePath, std::string suffix) {
  auto tmpPath = filePath;
  transform(tmpPath.begin(), tmpPath.end(), tmpPath.begin(), ::tolower);
  if (tmpPath.length() >= suffix.length()) {
    auto pos = tmpPath.find(suffix, tmpPath.length() - suffix.length());
    if (pos != std::string::npos) {
      return;
    }
  }
  filePath += suffix;
}

std::string AEUtils::GetItemName(const AEGP_ItemH& itemH) {
  if (itemH == nullptr) {
    return "";
  }
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  AEGP_MemHandle nameMemory;
  suites.ItemSuite8()->AEGP_GetItemName(pluginID, itemH, &nameMemory);
  std::string itemName = StringUtil::ToString(suites, nameMemory);
  suites.MemorySuite1()->AEGP_FreeMemHandle(nameMemory);

  itemName = StringUtil::DeleteLastSpace(itemName);
  return itemName;
}

std::string AEUtils::GetCompName(const AEGP_CompH& compH) {
  if (compH == nullptr) {
    return "";
  }
  auto itemH = GetItemFromComp(compH);
  return GetItemName(itemH);
}

std::string AEUtils::GetLayerName(const AEGP_LayerH& layerH) {
  if (layerH == nullptr) {
    return "";
  }
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  AEGP_MemHandle layerNameHandle;
  AEGP_MemHandle sourceNameHandle;
  suites.LayerSuite6()->AEGP_GetLayerName(pluginID, layerH, &layerNameHandle, &sourceNameHandle);
  auto name = StringUtil::ToString(suites, layerNameHandle);
  if (name.empty()) {
    name = StringUtil::ToString(suites, sourceNameHandle);
  }
  suites.MemorySuite1()->AEGP_FreeMemHandle(layerNameHandle);
  suites.MemorySuite1()->AEGP_FreeMemHandle(sourceNameHandle);

  name = StringUtil::DeleteLastSpace(name);
  return name;
}

void AEUtils::SetItemName(const AEGP_ItemH& itemH, std::string newName) {
  if (itemH == nullptr) {
    return;
  }
  auto& suites = SUITES();
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  auto fmtName = convert.from_bytes(newName);  // utf-8 to utf-16
  suites.ItemSuite8()->AEGP_SetItemName(itemH, (const A_UTF16Char*)fmtName.c_str());
}

uint32_t AEUtils::GetItemId(const AEGP_ItemH& itemH) {
  auto& suites = SUITES();
  A_long id = 0;
  if (itemH != nullptr) {
    suites.ItemSuite6()->AEGP_GetItemID(itemH, &id);
  }
  return static_cast<uint32_t>(id);
}

uint32_t AEUtils::GetItemIdFromLayer(const AEGP_LayerH& layerH) {
  auto itemH = GetItemFromLayer(layerH);
  auto id = GetItemId(itemH);
  return id;
}

uint32_t AEUtils::GetLayerId(const AEGP_LayerH& layerH) {
  auto& suites = SUITES();
  A_long id = 0;
  if (layerH != nullptr) {
    suites.LayerSuite6()->AEGP_GetLayerID(layerH, &id);
  }
  return static_cast<uint32_t>(id);
}

void AEUtils::SelectItem(const AEGP_ItemH& itemH) {
  auto& suites = SUITES();
  if (itemH != nullptr) {
    suites.ItemSuite6()->AEGP_SelectItem(itemH, true, true);
    suites.CommandSuite1()->AEGP_DoCommand(
        3061);  // 3061: Open selection, ignoring any modifier keys.
  }
}

void AEUtils::SelectItem(const AEGP_ItemH& itemH, const AEGP_LayerH& layerH) {
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  if (itemH == nullptr) {
    return;
  }

  bool hasLayer = (layerH != nullptr);

  AEGP_CollectionItemV2 collectionItem;
  AEGP_StreamRefH streamH;
  if (hasLayer) {
    suites.DynamicStreamSuite4()->AEGP_GetNewStreamRefForLayer(pluginID, layerH, &streamH);
    collectionItem.type = AEGP_CollectionItemType_LAYER;
    collectionItem.u.layer.layerH = layerH;
    collectionItem.stream_refH = streamH;
  }

  suites.ItemSuite6()->AEGP_SelectItem(itemH, true, true);
  if (!hasLayer) {
    suites.CommandSuite1()->AEGP_DoCommand(
        3061);  // 3061: Open selection, ignoring any modifier keys.
    return;
  }

  auto compH = GetCompFromItem(itemH);
  AEGP_Collection2H collectionH = nullptr;
  suites.CollectionSuite2()->AEGP_NewCollection(pluginID, &collectionH);
  suites.CollectionSuite2()->AEGP_CollectionPushBack(collectionH, &collectionItem);
  suites.CompSuite6()->AEGP_SetSelection(compH, collectionH);
  suites.CommandSuite1()->AEGP_DoCommand(
      3061);  // 3061: Open selection, ignoring any modifier keys.
  suites.CollectionSuite2()->AEGP_DisposeCollection(collectionH);
}

AEGP_CompH AEUtils::GetCompFromItem(const AEGP_ItemH& itemH) {
  auto& suites = SUITES();
  AEGP_CompH compH = nullptr;
  if (itemH != nullptr) {
    suites.CompSuite6()->AEGP_GetCompFromItem(itemH, &compH);
  }
  return compH;
}

AEGP_ItemH AEUtils::GetItemFromComp(const AEGP_CompH& compH) {
  auto& suites = SUITES();
  AEGP_ItemH itemH = nullptr;
  if (compH != nullptr) {
    suites.CompSuite6()->AEGP_GetItemFromComp(compH, &itemH);
  }
  return itemH;
}

AEGP_ItemH AEUtils::GetItemFromLayer(const AEGP_LayerH& layerH) {
  auto& suites = SUITES();
  AEGP_ItemH itemH = nullptr;
  if (layerH != nullptr) {
    suites.LayerSuite6()->AEGP_GetLayerSourceItem(layerH, &itemH);
  }
  return itemH;
}

int AEUtils::GetLayerNumEffects(const AEGP_LayerH& layerH) {
  auto& suites = SUITES();
  AEGP_LayerFlags layerFlags;
  suites.LayerSuite6()->AEGP_GetLayerFlags(layerH, &layerFlags);
  if ((layerFlags & AEGP_LayerFlag_EFFECTS_ACTIVE) == 0) {
    return 0;
  }
  A_long numEffects = 0;
  suites.EffectSuite4()->AEGP_GetLayerNumEffects(layerH, &numEffects);
  return static_cast<int>(numEffects);
}

static size_t GetContainStringPos(std::string str, std::string subStr) {
  if (str.length() < subStr.length()) {
    return std::string::npos;
  }
  transform(str.begin(), str.end(), str.begin(), ::tolower);
  transform(subStr.begin(), subStr.end(), subStr.begin(), ::tolower);
  auto pos = str.find(subStr, str.length() - subStr.length());
  return pos;
}

bool AEUtils::CheckIsBmp(std::string itemName) {
  if (itemName.length() >= SequenceSuffix.length()) {
    auto pos = GetContainStringPos(itemName, SequenceSuffix);
    if (pos != std::string::npos) {
      return true;
    }
  }
  return false;
}

bool AEUtils::CheckIsBmp(const AEGP_ItemH& itemH) {
  if (itemH == nullptr) {
    return false;
  }
  auto itemName = AEUtils::GetItemName(itemH);
  return CheckIsBmp(itemName);
}

void AEUtils::EnableBmp(const AEGP_ItemH& itemH) {
  if (itemH == nullptr) {
    return;
  }
  auto itemName = GetItemName(itemH);
  if (CheckIsBmp(itemName)) {
    return;
  }
  auto newName = itemName + SequenceSuffix;
  SetItemName(itemH, newName);
}

void AEUtils::DisableBmp(const AEGP_ItemH& itemH) {
  if (itemH == nullptr) {
    return;
  }
  auto itemName = GetItemName(itemH);
  if (!CheckIsBmp(itemName)) {
    return;
  }
  auto pos = GetContainStringPos(itemName, SequenceSuffix);
  if (pos != std::string::npos) {
    auto newName = itemName.substr(0, pos);
    SetItemName(itemH, newName);
  }
}

void AEUtils::ChangeBmp(const AEGP_ItemH& itemH) {
  if (itemH == nullptr) {
    return;
  }
  auto itemName = GetItemName(itemH);
  if (CheckIsBmp(itemName)) {
    DisableBmp(itemH);
  } else {
    EnableBmp(itemH);
  }
}

AEGP_InstalledEffectKey AEUtils::GetInstalledEffectKey(std::string name) {
  auto& suites = SUITES();
  A_long num = 0;
  suites.EffectSuite4()->AEGP_GetNumInstalledEffects(&num);
  AEGP_InstalledEffectKey oldKey = AEGP_InstalledEffectKey_NONE;
  AEGP_InstalledEffectKey key = AEGP_InstalledEffectKey_NONE;
  for (int i = 0; i < num; i++) {
    suites.EffectSuite4()->AEGP_GetNextInstalledEffect(oldKey, &key);
    A_char matchName[AEGP_MAX_EFFECT_MATCH_NAME_SIZE + 1];
    suites.EffectSuite4()->AEGP_GetEffectName(key, matchName);
    if (name == matchName) {
      return key;
    }
    oldKey = key;
  }
  return 0;
}

bool AEUtils::IsExistPath(std::string path) {
  return IsExistPath(path.c_str());
}

bool AEUtils::IsExistPath(const char* path) {
  if (0 == access(path, 0)) {
    return true;
  }
  return false;
}

float AEUtils::GetFrameRateFromComp(const AEGP_CompH& compH) {
  auto& suites = SUITES();
  A_FpLong frameRate = 24;
  suites.CompSuite6()->AEGP_GetCompFramerate(compH, &frameRate);
  return static_cast<float>(frameRate);
}

float AEUtils::GetFrameRateFromItem(const AEGP_ItemH& itemH) {
  auto compH = GetCompFromItem(itemH);
  return GetFrameRateFromComp(compH);
}

void AEUtils::ConvertFrameToTime(const AEGP_ItemH& itemH, int64_t frame, A_Time& keyTime) {
  float frameRate = AEUtils::GetFrameRateFromItem(itemH);
  keyTime.scale = static_cast<A_u_long>(frameRate > 1 ? frameRate : 1);
  keyTime.value = static_cast<A_long>(frame);
}

AEGP_StreamRefH AEUtils::GetMarkerStreamFromLayer(const AEGP_LayerH& layerH) {
  if (layerH == nullptr) {
    return nullptr;
  }
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  AEGP_StreamRefH streamH;
  suites.StreamSuite4()->AEGP_GetNewLayerStream(pluginID, layerH, AEGP_LayerStream_MARKER,
                                                &streamH);
  return streamH;
}

AEGP_StreamRefH AEUtils::GetMarkerStreamFromComposition(const AEGP_CompH& compH) {
  if (compH == nullptr) {
    return nullptr;
  }
  auto& suites = SUITES();
  auto pluginID = PLUGIN_ID();
  AEGP_StreamRefH streamH;
  suites.CompSuite10()->AEGP_GetNewCompMarkerStream(pluginID, compH, &streamH);
  return streamH;
}

AEGP_StreamRefH AEUtils::GetMarkerStreamFromItem(const AEGP_ItemH& itemH) {
  auto compH = AEUtils::GetCompFromItem(itemH);
  return GetMarkerStreamFromComposition(compH);
}

void AEUtils::DeleteStream(AEGP_StreamRefH& streamH) {
  if (streamH != nullptr) {
    auto& suites = SUITES();
    suites.StreamSuite4()->AEGP_DisposeStream(streamH);
    streamH = nullptr;
  }
}
