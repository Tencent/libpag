#pragma once
#include <memory>
#include "ImageCache.h"

namespace pag {
class JImageCacheHandle {
 public:
  explicit JImageCacheHandle(std::shared_ptr<ImageCache> nativeHandle)
      : nativeHandle(nativeHandle) {
  }

  std::shared_ptr<ImageCache> get() {
    return nativeHandle;
  }

  void reset() {
    nativeHandle = nullptr;
  }

 private:
  std::shared_ptr<ImageCache> nativeHandle;
};
}  // namespace pag