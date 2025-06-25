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

#pragma once
#include <memory>
#include <string>
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"

namespace AEHelper {

AEGP_ItemH GetActiveCompositionItem();

void SetSuitesAndPluginID(SPBasicSuite* basicSuite, AEGP_PluginID id);

std::shared_ptr<AEGP_SuiteHandler> GetSuites();

AEGP_PluginID GetPluginID();

std::string RunScript(std::shared_ptr<AEGP_SuiteHandler> suites, AEGP_PluginID pluginID,
                      const std::string& scriptText);

void RunScriptPreWarm();

bool CheckAeVersion();

std::string GetItemName(const AEGP_ItemH& itemH);

std::string GetCompName(const AEGP_CompH& compH);

std::string GetLayerName(const AEGP_LayerH& layerH);

AEGP_CompH GetCompFromItem(const AEGP_ItemH& itemH);

void SelectItem(const AEGP_ItemH& itemH);

void SelectItem(const AEGP_ItemH& itemH, const AEGP_LayerH& layerH);

const std::string TextDocumentScript = R"(
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

}  // namespace AEHelper
