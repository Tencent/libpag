#ifndef AE_UTILS_H
#define AE_UTILS_H

#include <string>
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
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
  static std::string BrowseForSave(bool useScript = false);
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

  static AEGP_SuiteHandler* GetSuites() {
    return Suites;
  }

  static AEGP_PluginID GetPluginID() {
    return PluginID;
  }
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

static const std::string TextDocumentScript = R"(
if (typeof PAG !== 'object') {
    PAG = {};
}
(function () {
    'use strict';
    PAG.printTextDocuments = function (compositionID, layerIndex, keyframeIndex) {
        var composition = null;
        for (var i = 1; i <= app.project.numItems; i++) {
            var item = app.project.item(i);
            if (item instanceof CompItem && item.id == compositionID) {
                composition = item;
                break;
            }
        }
        if (composition == null) {
            return "";
        }
        if (layerIndex >= composition.layers.length) {
            return "";
        }
        var textLayer = composition.layers[layerIndex + 1];
        var sourceText = textLayer.property("Source Text");
        if (!sourceText) {
            return "";
        }
        var textDocument;
        if (keyframeIndex === 0 && sourceText.numKeys === 0) {
            textDocument = sourceText.value;
        } else {
            textDocument = sourceText.keyValue(keyframeIndex + 1);
        }
        if (!textDocument) {
            return "";
        }
        var result = [];
        for (var key in textDocument) {
            if (!Object.prototype.hasOwnProperty.call(textDocument, key)) {
                continue;
            }
            try {
                var value = textDocument[key];
            } catch (e) {
                continue;
            }
            var text = key + " : ";
            switch (typeof value) {
                case 'string':
                    value = value.split("\x03").join("\n");
                    value = value.split("\r\n").join("\n");
                    value = value.split("\r").join("\n");
                    value = value.split("\n").join("\\n");
                    text += value;
                    break;
                case 'number':
                case 'boolean':
                    text += String(value);
                    break;
                case 'object':
                    if (value && Object.prototype.toString.apply(value) === '[object Array]') {
                        var partial = [];
                        var length = value.length;
                        for (var i = 0; i < length; i += 1) {
                            partial[i] = String(value[i]);
                        }
                        text += partial.join(',');
                    }
                    break;
            }
            if (text !== key + " : ") {
                result.push(text);
            }
        }
        return result.join("\n");
    }
}());
)";

#endif  // AE_UTILS_H
