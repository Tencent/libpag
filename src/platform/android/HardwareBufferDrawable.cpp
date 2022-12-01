#include "HardwareBufferDrawable.h"
#include <GLES/gl.h>

namespace pag {
std::shared_ptr<HardwareBufferDrawable> HardwareBufferDrawable::Make(
    int width, int height, std::shared_ptr<tgfx::Device> device) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  auto buffer = std::static_pointer_cast<tgfx::HardwareBuffer>(
      tgfx::HardwareBuffer::Make(width, height, false));
  if (!buffer) {
    return nullptr;
  }

  return std::shared_ptr<HardwareBufferDrawable>(
      new HardwareBufferDrawable(width, height, device, buffer));
}

void HardwareBufferDrawable::present(tgfx::Context*) {
  glFlush();
}

HardwareBufferDrawable::HardwareBufferDrawable(int width, int height,
                                               std::shared_ptr<tgfx::Device> device,
                                               std::shared_ptr<tgfx::HardwareBuffer> buffer)
    : _width(width), _height(height), device(device), hardwareBuffer(buffer) {
}

std::shared_ptr<tgfx::Surface> HardwareBufferDrawable::createSurface(tgfx::Context* context) {
  auto glTexture = hardwareBuffer->makeTexture(context);
  if (glTexture == nullptr) {
    return nullptr;
  }
  return tgfx::Surface::MakeFrom(std::move(glTexture));
}

}  // namespace pag
