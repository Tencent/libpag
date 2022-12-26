#include "HardwareBufferDrawable.h"
#include "HardwareBufferRenderTarget.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/gpu/opengl/GLDevice.h"
#include "tgfx/gpu/opengl/GLTexture.h"

namespace pag {
std::shared_ptr<HardwareBufferDrawable> HardwareBufferDrawable::Make(int width, int height) {
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  auto buffer = std::static_pointer_cast<tgfx::HardwareBuffer>(
      tgfx::HardwareBuffer::Make(width, height, false));
  if (!buffer) {
    return nullptr;
  }
  auto device = tgfx::GLDevice::Make();
  if (device == nullptr) {
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
  auto texture = hardwareBuffer->makeTexture(context);
  if (texture == nullptr) {
    return nullptr;
  }
  auto glTexture = std::static_pointer_cast<tgfx::GLTexture>(texture);
  auto renderTarget = HardwareBufferRenderTarget::MakeFrom(context, hardwareBuffer, texture);
  if (renderTarget == nullptr) {
    return nullptr;
  }
  return tgfx::Surface::MakeFrom(std::move(renderTarget), std::move(glTexture));
}

}  // namespace pag
