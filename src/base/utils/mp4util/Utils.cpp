//
// Created by katacai on 2022/3/28.
//

#include "Utils.h"
#include <chrono>

namespace pag {
int64_t Utils::getCurrentTimeMillis() {
  std::chrono::milliseconds ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch());
  return ms.count();
}

int64_t Utils::getCurrentTimeMicrosecond() {
  static auto START_TIME = std::chrono::high_resolution_clock::now();
  auto now = std::chrono::high_resolution_clock::now();
  auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now - START_TIME);
  return static_cast<int64_t>(ns.count() * 1e-3);
}
}
