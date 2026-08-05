/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#include <string>
#include "zip.h"

namespace pagx {

// Growable byte buffer with a read/write cursor, driving minizip through a
// custom zlib_filefunc_def so an archive can be assembled entirely in RAM.
// minizip seeks backward to patch each local file header's CRC / sizes once the
// entry is closed, so this must support seek/tell/overwrite in addition to
// append. Shared by PPTExporter and HTMLExporter (in-memory ZIP backends).
struct MemZipBuffer {
  std::string data;
  size_t position = 0;
};

zlib_filefunc_def MakeMemZipFileFunc(MemZipBuffer* buffer);

}  // namespace pagx
