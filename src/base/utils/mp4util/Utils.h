//
// Created by katacai on 2022/3/28.
//

#pragma once

#include <stdint.h>
#include "Utils.h"

namespace pag {
class Utils {
 public:
  static int64_t getCurrentTimeMillis();
  static int64_t getCurrentTimeMicrosecond();
};
}  // namespace pag
