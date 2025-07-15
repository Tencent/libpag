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

#include "base/utils/UniqueID.h"
#include "base/utils/Verify.h"
#include "pag/file.h"

namespace pag {
ImageBytes::ImageBytes() : uniqueID(UniqueID::Next()) {
}

ImageBytes::~ImageBytes() {
  delete cache;
  delete fileBytes;
}

bool ImageBytes::verify() const {
  VerifyAndReturn(fileBytes != nullptr && fileBytes->length() > 0 && scaleFactor > 0 && width > 0 &&
                  height > 0);
}
}  // namespace pag
