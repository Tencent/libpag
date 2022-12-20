#pragma once
#include "rendering/Drawable.h"
#include "tgfx/src/platform/android/HardwareBuffer.h"

namespace pag {
class HardwareBufferDrawable : public Drawable {
 public:
  static std::shared_ptr<HardwareBufferDrawable> Make(int width, int height,
                                                      std::shared_ptr<tgfx::Device> device);

  static std::shared_ptr<HardwareBufferDrawable> Make(std::shared_ptr<tgfx::HardwareBuffer> buffer,
                                                      std::shared_ptr<tgfx::Device> device);

  int width() const override {
    return _width;
  }
  int height() const override {
    return _height;
  }

  void updateSize() override {
  }

  std::shared_ptr<tgfx::Surface> createSurface(tgfx::Context* context) override;

  void present(tgfx::Context*) override;

  bool readPixels(tgfx::ColorType colorType, tgfx::AlphaType alphaType, void* dstPixels,
                  size_t dstRowBytes);

  std::shared_ptr<tgfx::Device> getDevice() override {
    return device;
  }

  AHardwareBuffer* aHardwareBuffer() {
    if (hardwareBuffer) {
      return hardwareBuffer->aHardwareBuffer();
    }
    return nullptr;
  }

 private:
  HardwareBufferDrawable(std::shared_ptr<tgfx::Device> device,
                         std::shared_ptr<tgfx::HardwareBuffer> buffer);
  int _width = 0;
  int _height = 0;
  std::shared_ptr<tgfx::Device> device = nullptr;
  std::shared_ptr<tgfx::HardwareBuffer> hardwareBuffer;
};

}  // namespace pag