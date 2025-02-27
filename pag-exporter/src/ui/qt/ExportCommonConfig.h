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
#ifndef EXPORTCOMMONCONFIG_H
#define EXPORTCOMMONCONFIG_H
#include <QtCore/Qt>
#include <Qtcore/QString>

static const int LEVEL_OFFSET_GAP = 36;
static const int UNFOLD_ICON_SIZE = 8;
static const int NORMAL_ICON_SIZE = 20;
static const int GAP_BETWEEN_NAME_AND_ICON = 13;
static const int LAYER_NAME_FONT_SIZE = 14;
static const QString PINGFANG_SC_FONT_FAMILY = QString::fromUtf8("PingFang SC");

static const int COLUMN_CHECKBOX = 0;
static const int COLUMN_LAYER_NAME = 1;
static const int COLUMN_SAVE_PATH = 2;
static const int COLUMN_SETTING = 3;
static const int COLUMN_PREVIEW = 4;

static const int COLUMN_CHECKBOX_WIDTH = 79;
static const int COLUMN_LAYER_NAME_WIDTH = 249;
static const int COLUMN_SETTING_WIDTH = 72;
static const int COLUMN_PREVIEW_WIDTH = 72;
static const int COLUMN_SAVE_PATH_WIDTH = 302;

static const auto LAYER_NAME_ROLE = Qt::UserRole + 1;      // 图层名
static const auto SAVE_PATH_ROLE = Qt::UserRole + 2;       // 存储路径
static const auto IS_FOLDER_ROLE = Qt::UserRole + 3;       // 是否文件夹
static const auto IS_UNFOLD_ROLE = Qt::UserRole + 4;       // 是否展开
static const auto LAYER_LEVEL_ROLE = Qt::UserRole + 5;     // 图层层级
static const auto AE_ITEM_DATA_ROLE = Qt::UserRole + 6;    // 图层层级
static const auto AE_ITEM_HEIGHT_ROLE = Qt::UserRole + 7;  // 图层高度

// 合成设置页
static const auto CAN_CHECK_ROLE = Qt::UserRole + 1;  // 是否可以改变BMP合成状态
static const auto OFFSET_X_ROLE = Qt::UserRole + 2;   // 头部偏移位置

// 占位图页
static const auto PLACEHOLDER_IMG_ROLE = Qt::UserRole + 1;         // 占位图图片资源
static const auto PLACEHOLDER_NAME_ROLE = Qt::UserRole + 2;        // 占位图名称
static const auto PLACEHOLDER_SCALE_MODE_ROLE = Qt::UserRole + 3;  // 占位图填充模式
static const auto PLACEHOLDER_ASSET_MODE_ROLE = Qt::UserRole + 4;  // 占位图素材格式
static const auto PLACEHOLDER_SELECTED_ROLE = Qt::UserRole + 5;    // 占位图是否选中

// 文本图层
static const auto TEXT_LAYER_NAME_ROLE = Qt::UserRole + 1;  // 文本图层名称

// 错误列表
static const auto ERROR_IS_ERROR_ROLE = Qt::UserRole + 1;
static const auto ERROR_INFO_ROLE = Qt::UserRole + 2;
static const auto ERROR_SUGGESTION_ROLE = Qt::UserRole + 3;
static const auto ERROR_IS_FOLD_ROLE = Qt::UserRole + 4;
static const auto ERROR_ITEM_HEIGHT_ROLE = Qt::UserRole + 5;
static const auto ERROR_HAS_SUGGESTION = Qt::UserRole + 6;
static const auto ERROR_COMPO_NAME_ROLE = Qt::UserRole + 7;
static const auto ERROR_LAYER_NAME_ROLE = Qt::UserRole + 8;
static const auto ERROR_FOLD_STATUS_CHANGE = Qt::UserRole + 9;
static const auto ERROR_HAS_LAYER_NAME = Qt::UserRole + 10;

// 导出进度页
static const auto PROGRESS_STATUS_CODE_ROLE = Qt::UserRole + 1;
static const auto PROGRESS_ERROR_INFO_ROLE = Qt::UserRole + 2;
static const auto PROGRESS_PAG_NAME_ROLE = Qt::UserRole + 3;
static const auto PROGRESS_TEXT_HEIGHT_ROLE = Qt::UserRole + 4;
static const auto PROGRESS_ITEM_HEIGHT_ROLE = Qt::UserRole + 5;
static const auto PROGRESS_PROGRESS_VALUE_ROLE = Qt::UserRole + 6;

#endif  //EXPORTCOMMONCONFIG_H
