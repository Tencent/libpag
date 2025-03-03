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
#ifndef AECOMPOSITIONPANEL_H
#define AECOMPOSITIONPANEL_H
#include <string>
#include <vector>
#include "src/exports/PAGDataTypes.h"
#include "AE_GeneralPlug.h"
#include "AEGP_SuiteHandler.h"
#include "AEResource.h"
#include "ImageData/ImageRgbaData.h"

#define PREVIEW_WIDTH_DEFAULT 720
#define PREVIEW_HEIGHT_DEFAULT 1280

struct AEComposition {
  AEGP_ItemH itemH;  // 合成资源句柄
  std::string name;  // 合成名字
  bool isBmp;  // 本合成是不是BMP合成
  bool notAllInBmp;  // 本合成的父合成（以及祖父合成）不是BMP合成 ——（合成面板里用来指示改子合成是显亮还是置灰）
};

class AECompositionPanel {
public:
  AECompositionPanel(AEGP_ItemH mainCompItemH);
  ~AECompositionPanel();
  void refresh(); // 改名后刷新
  std::vector<AEComposition>* getList(); // 获取子合成列表。-列表第一个为总合成本身，其它一次为子/孙合成
  std::vector<AEComposition>* getListWithRefresh();
  void printList();

  float frameRate = 24.0f;  // 帧率
  int duration = 0;  // 时长(帧数)
  ImageRgbaData previewImage;  // 缩略图数据(RGBA格式)
  std::string compositionName;
  std::shared_ptr<uint8_t> getPreviewImage(int frame,
                                           int width = PREVIEW_WIDTH_DEFAULT,
                                           int height = PREVIEW_HEIGHT_DEFAULT);
  void releaseImageRgbaData();

private:
  void pushToList(pag::ID compId);
  void pushCompositions(pag::Composition* composition);
  std::vector<AEComposition> list;
  pagexporter::Context context;
  std::vector<pag::Composition*> compositions;
};

// 依次打印所有合成的子合成列表
void PrintAllCompositionPanel(std::shared_ptr<AEResource> root);
#endif //AECOMPOSITIONPANEL_H
