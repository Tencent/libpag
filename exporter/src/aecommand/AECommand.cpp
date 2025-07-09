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

#include "AECommand.h"
#include "export/PAGExport.h"
#include "ui/AlertInfoModel.h"
#include "ui/WindowManager.h"
#include "utils/AEHelper.h"
#include "utils/AEResource.h"
#include "utils/AlertInfo.h"
namespace exporter {

AEGP_Command AECommand::PAGExporterCMD = 0L;
AEGP_Command AECommand::PAGConfigCMD = 0L;
AEGP_Command AECommand::PAGPanelCMD = 0L;
AEGP_Command AECommand::PAGPreviewCMD = 0L;

// 创建测试警告信息的函数
std::vector<AlertInfo> createTestWarnings() {
  std::vector<AlertInfo> testWarnings;

  // 1. 不支持的效果
  AlertInfo warning1(AlertInfoType::UnsupportedEffects, nullptr, nullptr, "Glow");
  warning1.compName = "Main Composition";
  warning1.layerName = "Effect Layer";
  testWarnings.push_back(warning1);

  // 2. 不支持的图层样式
  AlertInfo warning2(AlertInfoType::UnsupportedLayerStyle, nullptr, nullptr, "Drop Shadow");
  warning2.compName = "Main Composition";
  warning2.layerName = "Style Layer";
  testWarnings.push_back(warning2);

  // 3. 表达式不支持
  AlertInfo warning3(AlertInfoType::Expression, nullptr, nullptr);
  warning3.compName = "Animation Comp";
  warning3.layerName = "Animated Layer";
  testWarnings.push_back(warning3);

  // 4. 连续的BMP合成
  AlertInfo warning4(AlertInfoType::ContinuousSequence, nullptr, nullptr);
  warning4.compName = "Sequence Comp";
  warning4.layerName = "BMP Layer 1";
  testWarnings.push_back(warning4);

  // 5. 音频编码失败
  AlertInfo warning5(AlertInfoType::AudioEncodeFail, nullptr, nullptr);
  warning5.compName = "Audio Comp";
  warning5.layerName = "Audio Layer";
  testWarnings.push_back(warning5);

  // 6. 图形内存使用过高
  AlertInfo warning6(AlertInfoType::GraphicsMemory, nullptr, nullptr, "120MB");
  warning6.compName = "Heavy Comp";
  warning6.layerName = "Large Image Layer";
  testWarnings.push_back(warning6);

  // 7. 图片数量过多
  AlertInfo warning7(AlertInfoType::ImageNum, nullptr, nullptr, "45");
  warning7.compName = "Image Gallery";
  warning7.layerName = "Image Sequence";
  testWarnings.push_back(warning7);

  // 8. 字体文件过大
  AlertInfo warning8(AlertInfoType::FontFileTooBig, nullptr, nullptr, "15MB");
  warning8.compName = "Text Comp";
  warning8.layerName = "Title Text";
  testWarnings.push_back(warning8);

  // 9. 文本路径与文本动画不兼容
  AlertInfo warning9(AlertInfoType::TextPathAnimator, nullptr, nullptr);
  warning9.compName = "Text Animation";
  warning9.layerName = "Path Text";
  testWarnings.push_back(warning9);

  // 10. 文本动画范围选择器基于设置不支持
  AlertInfo warning10(AlertInfoType::RangeSelectorBasedOn, nullptr, nullptr, "Words");
  warning10.compName = "Text Animation";
  warning10.layerName = "Animated Text";
  testWarnings.push_back(warning10);

  return testWarnings;
}

A_Err AECommand::OnUpdateMenu(AEGP_GlobalRefcon /*globalRefcon*/,
                              AEGP_UpdateMenuRefcon /*menuRefcon*/,
                              AEGP_WindowType /*windowType*/) {
  A_Err err = A_Err_NONE;
  A_Err err2 = A_Err_NONE;
  const auto& suites = AEHelper::GetSuites();
  AEGP_ItemH active_itemH = AEHelper::GetActiveCompositionItem();
  if (active_itemH) {
    ERR(suites->CommandSuite1()->AEGP_EnableCommand(PAGExporterCMD));
    ERR(suites->CommandSuite1()->AEGP_EnableCommand(PAGPreviewCMD));
  } else {
    ERR2(suites->CommandSuite1()->AEGP_DisableCommand(PAGExporterCMD));
    ERR2(suites->CommandSuite1()->AEGP_DisableCommand(PAGPreviewCMD));
  }

  ERR(suites->CommandSuite1()->AEGP_EnableCommand(PAGConfigCMD));
  if (HasCompositionResource()) {
    ERR(suites->CommandSuite1()->AEGP_EnableCommand(PAGPanelCMD));
  } else {
    ERR2(suites->CommandSuite1()->AEGP_DisableCommand(PAGPanelCMD));
  }
  return err;
}

A_Err AECommand::OnClickConfig(AEGP_GlobalRefcon /*globalRefcon*/,
                               AEGP_CommandRefcon /*commandRefcon*/, AEGP_Command command,
                               AEGP_HookPriority /*hookPriority*/, A_Boolean /*alreadyHandled*/,
                               A_Boolean* handled) {
  A_Err err = A_Err_NONE;
  if (command != PAGConfigCMD) {
    return err;
  }
  *handled = TRUE;
  WindowManager::GetInstance().showPAGConfigWindow();
  return err;
}

A_Err AECommand::OnClickPanel(AEGP_GlobalRefcon /*globalRefcon*/,
                              AEGP_CommandRefcon /*commandRefcon*/, AEGP_Command command,
                              AEGP_HookPriority /*hookPriority*/, A_Boolean /*alreadyHandled*/,
                              A_Boolean* handled) {

  A_Err err = A_Err_NONE;
  if (command != PAGPanelCMD) {
    return err;
  }
  *handled = TRUE;
  return err;
}

A_Err AECommand::OnClickExporter(AEGP_GlobalRefcon /*globalRefcon*/,
                                 AEGP_CommandRefcon /*commandRefcon*/, AEGP_Command command,
                                 AEGP_HookPriority /*hookPriority*/, A_Boolean /*alreadyHandled*/,
                                 A_Boolean* handled) {
  A_Err err = A_Err_NONE;
  if (command != PAGExporterCMD) {
    return err;
  }
  *handled = TRUE;

  AEGP_ItemH activeItemH = AEHelper::GetActiveCompositionItem();
  if (activeItemH == nullptr) {
    return err;
  }

  AlertInfoModel alertModel;
  std::string outputPath = alertModel.browseForSave(true);

  if (outputPath.empty()) {
    return err;
  }
  std::vector<AlertInfo> testWarnings = createTestWarnings();
  WindowManager::GetInstance().showWarnings(testWarnings);

  return err;
}

A_Err AECommand::OnClickPreview(AEGP_GlobalRefcon /*globalRefcon*/,
                                AEGP_CommandRefcon /*commandRefcon*/, AEGP_Command command,
                                AEGP_HookPriority /*hookPriority*/, A_Boolean /*alreadyHandled*/,
                                A_Boolean* handled) {

  A_Err err = A_Err_NONE;
  if (command != PAGPreviewCMD) {
    return err;
  }
  *handled = TRUE;
  return err;
}

}  // namespace exporter
