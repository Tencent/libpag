#ifndef AE_UTILS_H
#define AE_UTILS_H

#include <string>
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"
#define PLUGIN_ID() AEUtils::GetPluginID()
#define SUITES() (*AEUtils::GetSuites())

class AEUtils {
 public:
  static std::string GetAeVersion();
  static std::string GetFolderDocumentsName();
  static std::string GetFolderTempName();
  static AEGP_ItemH GetActiveCompositionItem();
  static std::string GetProjectFileName();
  static std::string GetProjectPath();
  static std::string GetItemName(const AEGP_ItemH& itemH);
  static std::string GetCompName(const AEGP_CompH& compH);
  static std::string GetLayerName(const AEGP_LayerH& layerH);
  static void SetItemName(const AEGP_ItemH& itemH, std::string newName);
  static uint32_t GetItemId(const AEGP_ItemH& itemH);
  static uint32_t GetItemIdFromLayer(const AEGP_LayerH& layerH);
  static uint32_t GetLayerId(const AEGP_LayerH& layerH);
  static void SelectItem(const AEGP_ItemH& itemH);
  static void SelectItem(const AEGP_ItemH& itemH, const AEGP_LayerH& layerH);
  static AEGP_CompH GetCompFromItem(const AEGP_ItemH& itemH);
  static AEGP_ItemH GetItemFromComp(const AEGP_CompH& compH);
  static AEGP_ItemH GetItemFromLayer(const AEGP_LayerH& layerH);
  static float GetFrameRateFromComp(const AEGP_CompH& compH);
  static float GetFrameRateFromItem(const AEGP_ItemH& itemH);
  static void ConvertFrameToTime(const AEGP_ItemH& itemH, int64_t frame, A_Time& keyTime);
  static int GetLayerNumEffects(const AEGP_LayerH& layerH);
  static AEGP_InstalledEffectKey GetInstalledEffectKey(std::string name);
  static AEGP_StreamRefH GetMarkerStreamFromLayer(const AEGP_LayerH& layerH);
  static AEGP_StreamRefH GetMarkerStreamFromComposition(const AEGP_CompH& compH);
  static AEGP_StreamRefH GetMarkerStreamFromItem(const AEGP_ItemH& itemH);
  static void DeleteStream(AEGP_StreamRefH& streamH);

  static bool CheckIsBmp(std::string itemName);
  static bool CheckIsBmp(const AEGP_ItemH& itemH);
  static void EnableBmp(const AEGP_ItemH& itemH);
  static void DisableBmp(const AEGP_ItemH& itemH);
  static void ChangeBmp(const AEGP_ItemH& itemH);
    
  static bool IsExistPath(std::string path);
  static bool IsExistPath(const char* path);

  static void SetSuitesAndPluginID(SPBasicSuite* basicSuite, AEGP_PluginID id);

  static AEGP_SuiteHandler* GetSuites() { return Suites; }

  static AEGP_PluginID GetPluginID() { return PluginID; }
  static void GetLanguageString(char language[]);

  static void RunScriptPreWarm();
  static void RegisterTextDocumentScript();
  static bool RunOpenConfigPanelScript();

 private:
  static AEGP_PluginID PluginID;
  static AEGP_SuiteHandler* Suites;
  static std::string LastOutputPath;
  static std::string LastFilePath;
  static std::string SequenceSuffix;
  static std::string AeVersion;
  static std::string FolderDocumentsName;
  static std::string FolderTempName;
};

#endif  // AE_UTILS_H
