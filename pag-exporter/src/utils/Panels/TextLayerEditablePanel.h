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
#ifndef TEXTLAYEREDITABLEPANEL_H
#define TEXTLAYEREDITABLEPANEL_H
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "src/exports/PAGDataTypes.h"
#include <string>
#include <vector>

class PlaceTextPanel;

class PlaceTextLayer {
public:
  PlaceTextLayer(pagexporter::Context& context, PlaceTextPanel* mainPanel, AEGP_LayerH layerH);
  ~PlaceTextLayer();

  // 方便UI读取才写成public，但请上层不要修改下列数据
  AEGP_LayerH layerH;  // 图层句柄
  std::string name = "";  // 图层名字
  bool editableFlag = true;  // 文本图层默认可以编辑

  void setEditable(bool flag);
  bool getEditable();
private:
  pagexporter::Context& context;
  PlaceTextPanel* mainPanel = nullptr;

  friend class PlaceImagePanel;
};

class PlaceTextPanel {
public:
  PlaceTextPanel(AEGP_ItemH mainCompItemH);
  ~PlaceTextPanel();

  // 获取占位图层列表。
  std::vector<PlaceTextLayer>* getList();

  // 刷新图层列表。
  void refresh();

  AEGP_ItemH mainCompItemH = nullptr;
  std::vector<PlaceTextLayer> list;

private:
  pagexporter::Context* context = nullptr;
};

#endif //TEXTLAYEREDITABLEPANEL_H
