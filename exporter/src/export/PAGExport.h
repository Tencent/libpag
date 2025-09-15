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
class PAGExport {
 public:
  static bool ExportFile(const AEGP_ItemH& activeItemH, const std::string& outputPath,
                         bool exportAudio = true, bool hardwareEncode = false);
  static bool ExportFile(PAGExport* pagExport);
  static bool ValidatePAGFile(uint8_t* data, size_t size);

  PAGExport(const AEGP_ItemH& activeItemH, const std::string& outputPath, bool exportAudio = true,
            bool hardwareEncode = false);
  std::shared_ptr<pag::File> exportAsFile();

  AEGP_ItemH itemH = nullptr;
  std::shared_ptr<PAGExportSession> session = nullptr;
  ScopedTimeSetter timeSetter = {nullptr, 0};

 private:
  void addRootComposition() const;
  std::vector<pag::ImageBytes*> getRefImages(const std::vector<pag::Composition*>& compositions);
  void exportResources(std::vector<pag::Composition*>& compositions);
};
}  // namespace exporter
