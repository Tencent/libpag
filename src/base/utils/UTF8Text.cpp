/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "UTF8Text.h"

namespace pag {
int UTF8Text::Count(const std::string& string) {
    if (string.empty()) {
        return -1;
    }
    int count = 0;
    const char* start = &(string[0]);
    const char* stop = start + string.size();
    while (start < stop) {
        NextChar(&start);
        ++count;
    }
    return count;
}

static inline int32_t LeftShift(int32_t value) {
    return static_cast<int32_t>((static_cast<uint32_t>(value) << 1));
}

int32_t UTF8Text::NextChar(const char** ptr) {
    auto p = (const uint8_t*)*ptr;
    int c = *p;
    int hic = c << 24;
    if (hic < 0) {
        auto mask = static_cast<uint32_t>(~0x3F);
        hic = LeftShift(hic);
        do {
            c = (c << 6) | (*++p & 0x3F);
            mask <<= 5;
        } while ((hic = LeftShift(hic)) < 0);
        c &= ~mask;
    }
    *ptr = static_cast<const char*>(static_cast<const void*>(p + 1));
    return c;
}
}  // namespace pag