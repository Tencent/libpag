//
// Created on 2024/7/8.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#pragma once

#include "platform/Platform.h"

namespace pag {
class NativePlatform : public Platform {
 public:
  static void InitJNI();

  std::vector<const VideoDecoderFactory*> getVideoDecoderFactories() const override;

  bool registerFallbackFonts() const override;

  void traceImage(const tgfx::ImageInfo& info, const void* pixels,
                  const std::string& tag) const override;

  std::string getCacheDir() const override;

  std::shared_ptr<DisplayLink> createDisplayLink(std::function<void()> callback) const override;
};
}  // namespace pag
