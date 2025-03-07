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
#ifndef PAGEXPORTER_H
#define PAGEXPORTER_H
#include "src/exports/Composition/Composition.h"
#include "src/exports/ProjectPlayTime.h"
#include "src/ui/qt/ProgressBase.h"

class PAGExporter {
 public:
  // 将合成导出成PAG文件
  static bool ExportFile(const AEGP_ItemH& activeItemH, std::string outputPath,
                         bool enableAudio = true, bool enableBeforeWarning = false,
                         QString titleName = "", QString progressTips = "",
                         ProgressBase* progressBase = nullptr,
                         std::vector<pagexporter::AlertInfo>* errorInfos = nullptr,
                         bool bHardware = false);

  static std::vector<pag::Composition*> ResortCompositions(const pagexporter::Context& context);
  static std::vector<pag::ImageBytes*> ResortImages(const pagexporter::Context& context, const std::vector<pag::Composition*>& compositions);

  PAGExporter(const AEGP_ItemH& activeItemH, std::string outputPath,
              ProgressBase* progressBase = nullptr);

  ~PAGExporter() = default;

 private:
  std::shared_ptr<pag::File> ExportPAG(const AEGP_ItemH& activeItemH);
  void exporterRescale(std::vector<pag::Composition*>& compositions);
  void exportRescaleImages();
  void exportRescaleBitmapCompositions(const std::vector<pag::Composition*>& compositions);
  void exportRescaleVideoCompositions(std::vector<pag::Composition*>& compositions);
  void processWorkArea(const AEGP_ItemH& activeItemH);

  pagexporter::Context context;
  ProjectPlayTime projectPlayTime;
};
#endif  //PAGEXPORTER_H
