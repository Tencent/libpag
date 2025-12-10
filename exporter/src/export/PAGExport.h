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

#pragma once
#include <memory>
#include "utils/AEHelper.h"
#include "utils/AlertInfo.h"
#include "utils/PAGExportSession.h"
#include "utils/ScopedHelper.h"

namespace exporter {

struct PAGExportConfigParam {
  bool exportAudio = true;
  bool hardwareEncode = false;
  bool exportActually = true;
  bool showAlertInfo = false;
  AEGP_ItemH activeItemHandle = nullptr;
  std::string outputPath = "";
};

class PAGExport {
 public:
  explicit PAGExport(const PAGExportConfigParam& configParam);

  bool exportFile();

  bool canceled = false;
  AEGP_ItemH itemHandle = nullptr;
  std::shared_ptr<PAGExportSession> session = nullptr;
  ScopedTimeSetter timeSetter = {nullptr, 0};

 private:
  std::shared_ptr<pag::File> exportAsFile();
  void addRootComposition() const;
  std::vector<pag::ImageBytes*> getRefImages(const std::vector<pag::Composition*>& compositions);
  void exportResources(std::vector<pag::Composition*>& compositions);
  void exportRescaleImages() const;
  void exportRescaleBitmapCompositions(std::vector<pag::Composition*>& compositions) const;
  void exportRescaleVideoCompositions(std::vector<pag::Composition*>& compositions) const;
};
}  // namespace exporter
