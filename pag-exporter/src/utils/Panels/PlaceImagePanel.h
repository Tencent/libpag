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
#ifndef PLACEIMAGEPANEL_H
#define PLACEIMAGEPANEL_H
#include "AEGP_SuiteHandler.h"
#include "AE_GeneralPlug.h"
#include "ImageData/ImageRgbaData.h"
#include "src/exports/PAGDataTypes.h"
#include <QtCore/QMetaType>
#include <string>
#include <vector>

#define THUMBNAIL_WIDTH_DEFAULT 48
#define THUMBNAIL_HEIGHT_DEFAULT 48

// 填充模式
enum class ImageFillMode {
  None = 0, // 不缩放
  Stretch, // 拉伸
  LetterBox, // 黑边
  Zoom, // 裁剪
  NotFind,  // 此值仅内部使用, 对外部来说找不到时会默认 LetterBox
  Default = LetterBox
};
Q_DECLARE_METATYPE(ImageFillMode)

// 替换素材的格式
enum class FillResourceType {
  Replace = 0,  // 默认，可以替换图片或视频
  NoReplace,  // 不允许替换
};
Q_DECLARE_METATYPE(FillResourceType)

class PlaceImagePanel;

class PlaceImageLayer {
 public:
  PlaceImageLayer(pagexporter::Context& context, PlaceImagePanel* mainPanel, AEGP_LayerH layerH);
  ~PlaceImageLayer();

  static bool IsNoReplaceImageLayer(pagexporter::Context& context, const AEGP_ItemH& mainCompItemH, const AEGP_LayerH& layerH);
  static ImageFillMode GetFillModeFromLayer(const AEGP_LayerH& layerH);
  static ImageFillMode GetFillModeFromComposition(const AEGP_ItemH& mainCompItemH, pag::ID imageId);

  ImageFillMode getFillMode();  // 获取填充模式
  void setFillMode(ImageFillMode mode);  // 设置填充模式
  void setResourceType(FillResourceType type);  // 设置替换素材格式

  // 方便UI读取才写成public，但请上层不要修改下列数据
  AEGP_ItemH imageH;  // 图片资源句柄
  std::vector<AEGP_LayerH> layerHs;  // 图层句柄
  std::string name = "";  // 占位图/图层名字
  bool editableFlag = true;  // 图片默认可以替换
  bool isVideo = false;  // 是否视频占位图(从视频图层而来)
  ImageFillMode fillMode = ImageFillMode::Default;  // 填充模式
  FillResourceType resourceType = FillResourceType::Replace;  // 素材格式
  ImageRgbaData thumbnail;  // 缩略图数据(RGBA格式)
  std::shared_ptr<uint8_t> refreshThumbnail(int width = THUMBNAIL_WIDTH_DEFAULT,
                                            int height = THUMBNAIL_WIDTH_DEFAULT);
  bool isSelected = false;

 private:
  void getFillResourceType();
  void addImageFillRule();
  void deleteImageFillRule();
  pagexporter::Context& context;
  PlaceImagePanel* mainPanel = nullptr;

  friend class PlaceImagePanel;
};

class PlaceImagePanel {
 public:
  PlaceImagePanel(AEGP_ItemH mainCompItemH);
  ~PlaceImagePanel();

  // 获取占位图层列表。
  std::vector<PlaceImageLayer>* getList();

  // 刷新占位图列表。
  void refresh();

  void setResourceType(int index, FillResourceType type);  // 设置替换素材格式

  std::vector<PlaceImageLayer> list;
  AEGP_ItemH mainCompItemH = nullptr;

 private:
  pagexporter::Context* context = nullptr;
};

#endif //PLACEIMAGEPANEL_H
