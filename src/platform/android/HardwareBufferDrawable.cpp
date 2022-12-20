#include "HardwareBufferDrawable.h"
#include <GLES/gl.h>
#include "tgfx/core/Bitmap.h"

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

  return std::shared_ptr<HardwareBufferDrawable>(new HardwareBufferDrawable(device, buffer));
}

std::shared_ptr<HardwareBufferDrawable> HardwareBufferDrawable::Make(
    std::shared_ptr<tgfx::HardwareBuffer> buffer, std::shared_ptr<tgfx::Device> device) {
  if (buffer == nullptr || buffer->width() <= 0 || buffer->height() <= 0) {
    return nullptr;
  }
  return std::shared_ptr<HardwareBufferDrawable>(new HardwareBufferDrawable(device, buffer));
}

void HardwareBufferDrawable::present(tgfx::Context*) {
}

bool HardwareBufferDrawable::readPixels(tgfx::ColorType colorType, tgfx::AlphaType alphaType,
                                        void* dstPixels, size_t dstRowBytes) {
  auto srcPixels = hardwareBuffer->lockPixels();
  if (!srcPixels) {
    return false;
  }
  auto dstInfo = tgfx::ImageInfo::Make(_width, _height, colorType, alphaType, dstRowBytes);
  tgfx::Bitmap bitmap(hardwareBuffer->info(), srcPixels);
  bitmap.readPixels(dstInfo, dstPixels);
  hardwareBuffer->unlockPixels();
  return true;
}

HardwareBufferDrawable::HardwareBufferDrawable(std::shared_ptr<tgfx::Device> device,
                                               std::shared_ptr<tgfx::HardwareBuffer> buffer)
    : _width(buffer->width()), _height(buffer->height()), device(device), hardwareBuffer(buffer) {
}

std::shared_ptr<tgfx::Surface> HardwareBufferDrawable::createSurface(tgfx::Context* context) {
  auto glTexture = hardwareBuffer->makeTexture(context);
  if (glTexture == nullptr) {
    return nullptr;
  }
  return tgfx::Surface::MakeFrom(std::move(glTexture));
}

}  // namespace pag
