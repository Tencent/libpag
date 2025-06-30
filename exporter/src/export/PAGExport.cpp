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

#include "PAGExport.h"
#include <fstream>
#include <iostream>
#include "ExportComposition.h"
#include "utils/AEHelper.h"
namespace exporter {

PAGExport::PAGExport(const AEGP_ItemH& activeItemH, std::string& outputPath)
    : session(std::make_shared<PAGExportSession>(outputPath)), timeSetter(activeItemH, -100.0f) {
}

bool PAGExport::ExportFile(const AEGP_ItemH& activeItemH, std::string& outputPath,
                           bool enableAudio) {
  bool res = false;
  do {
    if (activeItemH == nullptr || outputPath.empty()) {
      break;
    }
    PAGExport pagExport(activeItemH, outputPath);
    pagExport.session->enableAudio = enableAudio;

    auto pagFile = pagExport.exportPAG(activeItemH);
    if (pagFile == nullptr) {
      break;
    }

    const auto bytes = pag::Codec::Encode(pagFile);
    if (bytes->length() == 0) {
      break;
    }
    if (!WriteToFile(outputPath, reinterpret_cast<char*>(bytes->data()),
                     static_cast<std::streamsize>(bytes->length()))) {
      break;
    }

    const auto pagFileDecoded = pag::File::Load(bytes->data(), bytes->length());
    if (pagFileDecoded == nullptr) {
      break;
    }
    const auto bytes2 = pag::Codec::Encode(pagFileDecoded);
    if (bytes2->length() != bytes->length()) {
      printf("warning: bytes2->length(%zu) != bytes->length(%zu)\n", bytes2->length(),
             bytes->length());
    }
    res = true;
  } while (false);

  return res;
}

std::shared_ptr<pag::File> PAGExport::exportPAG(const AEGP_ItemH& activeItemH) {

  auto id = AEHelper::GetItemId(activeItemH);
  ScopedAssign<pag::ID> arCI(session->curCompId, id);
  ExportComposition(session, activeItemH);

  return nullptr;
}

bool PAGExport::WriteToFile(const std::string& filePath, const char* data, std::streamsize size) {
  std::ofstream file(filePath, std::ios::binary);
  if (!file) {
    std::cerr << "Failed to open file: " << filePath << std::endl;
    return false;
  }

  file.write(data, size);

  if (file.fail()) {
    std::cerr << "Failed to write to file: " << filePath << std::endl;
    if (file.eof()) {
      std::cerr << "eof is set: End-of-File reached on input operation." << std::endl;
    }
    file.close();
    return false;
  }

  file.close();
  return true;
}

}  // namespace exporter
