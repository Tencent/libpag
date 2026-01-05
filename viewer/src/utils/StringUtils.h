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

#include <QString>
#include "pag/types.h"

namespace pag::Utils {

QString ToQString(double num);
QString ToQString(int32_t num);
QString ToQString(int64_t num);
QString GetMemorySizeUnit(int64_t size);
QString GetMemorySizeNumString(int64_t size);
std::string TagCodeToVersion(uint16_t tagCode);
Color QStringToColor(const QString& color);
QString ColorToQString(const Color& color);

}  // namespace pag::Utils
