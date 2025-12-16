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

#include <pag/file.h>
#include "utils/PAGExportSession.h"

namespace exporter {

void CheckBeforeExport(std::shared_ptr<PAGExportSession> session,
                       std::vector<pag::Composition*>& compositions,
                       std::vector<pag::ImageBytes*>& images);

void CheckAfterExport(std::shared_ptr<PAGExportSession> session,
                      std::vector<pag::Composition*>& compositions);

void CheckGraphicsMemory(std::shared_ptr<PAGExportSession> session,
                         std::shared_ptr<pag::File> pagFile);

}  // namespace exporter
