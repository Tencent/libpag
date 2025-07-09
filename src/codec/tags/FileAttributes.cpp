/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "FileAttributes.h"

namespace pag {

void ReadFileAttributes(DecodeStream* stream, pag::FileAttributes* fileAttributes) {
  fileAttributes->timestamp = stream->readEncodedInt64();
  fileAttributes->pluginVersion = stream->readUTF8String();
  fileAttributes->aeVersion = stream->readUTF8String();
  fileAttributes->systemVersion = stream->readUTF8String();
  fileAttributes->author = stream->readUTF8String();
  fileAttributes->scene = stream->readUTF8String();

  auto warningCount = static_cast<int>(stream->readEncodedUint32());
  for (int i = 0; i < warningCount; i++) {
    if (stream->context->hasException()) {
      break;
    }
    auto warning = stream->readUTF8String();
    fileAttributes->warnings.push_back(warning);
  }
}

TagCode WriteFileAttributes(EncodeStream* stream, pag::FileAttributes* fileAttributes) {
  stream->writeEncodedInt64(fileAttributes->timestamp);
  stream->writeUTF8String(fileAttributes->pluginVersion);
  stream->writeUTF8String(fileAttributes->aeVersion);
  stream->writeUTF8String(fileAttributes->systemVersion);
  stream->writeUTF8String(fileAttributes->author);
  stream->writeUTF8String(fileAttributes->scene);

  stream->writeEncodedUint32(static_cast<uint32_t>(fileAttributes->warnings.size()));
  for (auto& warning : fileAttributes->warnings) {
    stream->writeUTF8String(warning);
  }

  return TagCode::FileAttributes;
}
}  // namespace pag
