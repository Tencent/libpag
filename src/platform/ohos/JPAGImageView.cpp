/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

#include "JPAGImageView.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include <multimedia/image_framework/image/pixelmap_native.h>
#include <multimedia/image_framework/image_pixel_map_mdk.h>
#include <native_buffer/native_buffer.h>
#include <cstdint>
#include "base/utils/TGFXCast.h"
#include "base/utils/TimeUtil.h"
#include "base/utils/UniqueID.h"
#include "platform/ohos/GPUDrawable.h"
#include "platform/ohos/JPAG.h"
#include "platform/ohos/JPAGLayerHandle.h"
#include "platform/ohos/JsHelper.h"
#include "rendering/utils/ApplyScaleMode.h"
#include "tgfx/core/ColorType.h"
#include "tgfx/core/Task.h"
#include "tgfx/platform/ohos/OHOSPixelMap.h"

namespace pag {
static std::unordered_map<std::string, std::shared_ptr<JPAGImageView>> ViewMap = {};
static std::mutex ViewMapLocker = {};

class JPAGImageViewRenderSession
    : public std::enable_shared_from_this<JPAGImageViewRenderSession> {
 public:
  ~JPAGImageViewRenderSession() {
    clearSurface();
  }

  bool updateFrame(double progress) {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    if (released.load()) {
      return false;
    }
    auto decoder = getDecoder();
    if (decoder == nullptr) {
      return false;
    }
    return handleFrame(ProgressToFrame(progress, decoder->numFrames()));
  }

  void setWindow(NativeWindow* window) {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    if (released.load()) {
      if (window != nullptr) {
        OH_NativeWindow_NativeObjectUnreference(window);
      }
      return;
    }
    clearSurface();
    nativeWindow = window;
    targetWindow = MakeEGLWindow(nativeWindow);
    invalidSize();
  }

  void updateSize() {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    if (!released.load()) {
      invalidSize();
    }
  }

  void clearWindow() {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    clearSurface();
  }

  void setComposition(std::shared_ptr<PAGComposition> value, float valueFrameRate) {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    if (released.load()) {
      return;
    }
    composition = std::move(value);
    frameRate = valueFrameRate;
    invalidDecoder();
  }

  Frame numFrames() {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    auto decoder = released.load() ? nullptr : getDecoder();
    return decoder == nullptr ? 0 : decoder->numFrames();
  }

  void setScaleMode(PAGScaleMode value) {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    scaleMode = value;
    refreshMatrixFromScaleMode();
  }

  PAGScaleMode getScaleMode() {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    return scaleMode;
  }

  void setMatrix(const Matrix& value) {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    matrix = ToTGFX(value);
    scaleMode = PAGScaleMode::None;
  }

  Matrix getMatrix() {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    return ToPAG(matrix);
  }

  void setRenderScale(float value) {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    if (value <= 0.0 || value > 1.0) {
      value = 1.0;
    }
    if (renderScale == value) {
      return;
    }
    renderScale = value;
    invalidDecoder();
  }

  float getRenderScale() {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    return renderScale;
  }

  void setCacheAllFramesInMemory(bool value) {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    if (cacheAllFramesInMemory == value) {
      return;
    }
    cacheAllFramesInMemory = value;
    if (!value) {
      images.clear();
    }
  }

  bool getCacheAllFramesInMemory() {
    std::lock_guard<std::mutex> autoLock(operationLocker);
    return cacheAllFramesInMemory;
  }

  tgfx::Bitmap getCurrentBitmap() {
    std::lock_guard<std::mutex> autoLock(imageLocker);
    return currentBitmap;
  }

  void release() {
    if (released.exchange(true)) {
      return;
    }
    auto session = shared_from_this();
    tgfx::Task::Run([session]() {
      std::lock_guard<std::mutex> autoLock(session->operationLocker);
      session->clearSurface();
      session->invalidDecoder();
      session->composition = nullptr;
      std::lock_guard<std::mutex> imageLock(session->imageLocker);
      session->currentBitmap = {};
      session->currentImage = nullptr;
    });
  }

 private:
  std::mutex operationLocker = {};
  std::mutex imageLocker = {};
  std::atomic_bool released = false;
  int width = 0;
  int height = 0;
  float renderScale = 1.0f;
  float frameRate = 30.0f;
  PAGScaleMode scaleMode = PAGScaleMode::LetterBox;
  tgfx::Matrix matrix = tgfx::Matrix::I();
  bool cacheAllFramesInMemory = false;
  std::shared_ptr<PAGComposition> composition = nullptr;
  std::shared_ptr<PAGDecoder> decoder = nullptr;
  NativeWindow* nativeWindow = nullptr;
  std::shared_ptr<tgfx::Window> targetWindow = nullptr;
  std::shared_ptr<tgfx::Surface> renderSurface = nullptr;
  std::shared_ptr<tgfx::Image> currentImage = nullptr;
  tgfx::Bitmap currentBitmap = {};
  std::unordered_map<Frame, std::pair<tgfx::Bitmap, std::shared_ptr<tgfx::Image>>> images = {};

  std::shared_ptr<PAGDecoder> getDecoder() {
    if (targetWindow == nullptr || composition == nullptr || released.load()) {
      invalidDecoder();
      return nullptr;
    }
    if (decoder == nullptr) {
      float scaleFactor = 1.0f;
      if (width >= height) {
        scaleFactor = renderScale * static_cast<float>(width) / composition->width();
      } else {
        scaleFactor = renderScale * static_cast<float>(height) / composition->height();
      }
      decoder = PAGDecoder::MakeFrom(composition, frameRate, scaleFactor);
      refreshMatrixFromScaleMode();
    }
    return decoder;
  }

  void invalidSize() {
    if (targetWindow != nullptr && nativeWindow != nullptr) {
      renderSurface = nullptr;
      OH_NativeWindow_NativeWindowHandleOpt(nativeWindow, GET_BUFFER_GEOMETRY, &height, &width);
    } else {
      width = 0;
      height = 0;
    }
    invalidDecoder();
  }

  void clearSurface() {
    renderSurface = nullptr;
    targetWindow = nullptr;
    if (nativeWindow != nullptr) {
      OH_NativeWindow_NativeObjectUnreference(nativeWindow);
      nativeWindow = nullptr;
    }
    invalidSize();
  }

  void invalidDecoder() {
    decoder = nullptr;
    images.clear();
  }

  void refreshMatrixFromScaleMode() {
    if (decoder == nullptr || scaleMode == PAGScaleMode::None) {
      return;
    }
    matrix = ToTGFX(ApplyScaleMode(scaleMode, decoder->width() / renderScale,
                                   decoder->height() / renderScale, width, height));
  }

  bool handleFrame(Frame frame) {
    auto currentDecoder = getDecoder();
    if (currentDecoder == nullptr) {
      return false;
    }
    if (!currentDecoder->checkFrameChanged(frame) && currentImage != nullptr) {
      return present(currentImage);
    }
    auto image = getImage(frame);
    if (image.second == nullptr || released.load()) {
      return false;
    }
    if (!present(image.second)) {
      return false;
    }
    {
      std::lock_guard<std::mutex> autoLock(imageLocker);
      currentBitmap = image.first;
      currentImage = image.second;
    }
    return true;
  }

  std::pair<tgfx::Bitmap, std::shared_ptr<tgfx::Image>> getImage(Frame frame) {
    if (cacheAllFramesInMemory) {
      auto result = images.find(frame);
      if (result != images.end()) {
        return result->second;
      }
    }
    tgfx::Bitmap bitmap = {};
    if (decoder == nullptr || !bitmap.allocPixels(decoder->width(), decoder->height(), false, false)) {
      return {{}, nullptr};
    }
    auto pixels = bitmap.lockPixels();
    if (pixels == nullptr) {
      return {{}, nullptr};
    }
    auto success = decoder->readFrame(frame, pixels, bitmap.rowBytes());
    bitmap.unlockPixels();
    if (!success) {
      return {{}, nullptr};
    }
    auto image = tgfx::Image::MakeFrom(bitmap);
    if (image != nullptr && cacheAllFramesInMemory) {
      images[frame] = {bitmap, image};
    }
    return {bitmap, image};
  }

  bool present(const std::shared_ptr<tgfx::Image>& image) {
    if (targetWindow == nullptr || image == nullptr || released.load()) {
      return false;
    }
    auto device = targetWindow->getDevice();
    if (device == nullptr) {
      return false;
    }
    auto context = device->lockContext();
    if (context == nullptr) {
      return false;
    }
    if (renderSurface == nullptr) {
      renderSurface = tgfx::Surface::MakeFrom(context, targetWindow);
    }
    auto surface = renderSurface;
    if (surface == nullptr) {
      device->unlock();
      return false;
    }
    auto canvas = surface->getCanvas();
    if (canvas == nullptr) {
      device->unlock();
      return false;
    }
    canvas->clear();
    auto imageMatrix = tgfx::Matrix::MakeScale(1.0 / renderScale);
    imageMatrix.postConcat(matrix);
    canvas->save();
    canvas->concat(imageMatrix);
    canvas->drawImage(image);
    canvas->restore();
    context->flushAndSubmit();
    context->purgeResourcesNotUsedSince(std::chrono::steady_clock::now());
    device->unlock();
    return true;
  }
};

static napi_value Flush(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  bool flushResult = false;
  if (view != nullptr) {
    flushResult = view->flush();
  }
  napi_value result;
  napi_get_boolean(env, flushResult, &result);
  return result;
}

static napi_value Update(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);

  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  auto animator = view->getAnimator();
  if (animator != nullptr) {
    animator->update();
  }
  return nullptr;
}

static napi_value SetComposition(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 2;
  napi_value args[2] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc < 2) {
    return nullptr;
  }
  auto layer = JPAGLayerHandle::FromJs(env, args[0]);
  double frameRate = 30.0f;
  napi_get_value_double(env, args[1], &frameRate);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    if (layer != nullptr && layer->layerType() == LayerType::PreCompose) {
      view->setComposition(std::static_pointer_cast<PAGComposition>(layer), frameRate);
    } else {
      view->setComposition(nullptr, 30.0f);
    }
  }
  return nullptr;
}

static napi_value ScaleMode(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  PAGScaleMode scaleMode = PAGScaleMode::LetterBox;
  if (view != nullptr) {
    scaleMode = view->scaleMode();
  }
  napi_value result;
  napi_create_int32(env, static_cast<int32_t>(scaleMode), &result);
  return result;
}

static napi_value SetScaleMode(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  int scaleMode = 0;
  napi_get_value_int32(env, args[0], &scaleMode);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setScaleMode(static_cast<PAGScaleMode>(scaleMode));
  }
  return nullptr;
}

static napi_value Matrix(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  auto matrix = Matrix::I();
  if (view != nullptr) {
    matrix = view->matrix();
  }
  return CreateMatrix(env, matrix);
}

static napi_value SetMatrix(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setMatrix(GetMatrix(env, args[0]));
  }
  return nullptr;
}

static napi_value CacheAllFramesInMemory(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  bool cacheAllFramesInMemory = false;
  if (view != nullptr) {
    cacheAllFramesInMemory = view->cacheAllFramesInMemory();
  }
  napi_value result;
  napi_get_boolean(env, cacheAllFramesInMemory, &result);
  return result;
}

static napi_value SetCacheAllFramesInMemory(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  bool cacheAllFramesInMemory = false;
  napi_get_value_bool(env, args[0], &cacheAllFramesInMemory);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setCacheAllFramesInMemory(cacheAllFramesInMemory);
  }
  return nullptr;
}

static napi_value RepeatCount(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  int repeatCount = 0;
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator != nullptr) {
      repeatCount = animator->repeatCount();
    }
  }
  napi_value result;
  napi_create_int32(env, repeatCount, &result);
  return result;
}

static napi_value SetRepeatCount(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  int repeatCount = 0;
  napi_get_value_int32(env, args[0], &repeatCount);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator) {
      animator->setRepeatCount(repeatCount);
    }
  }
  return nullptr;
}

static napi_value Play(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator) {
      animator->start();
    }
  }
  return nullptr;
}

static napi_value IsPlaying(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  bool isPlaying = false;
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator != nullptr) {
      isPlaying = animator->isRunning();
    }
  }
  napi_value result;
  napi_get_boolean(env, isPlaying, &result);
  return result;
}

static napi_value Pause(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    auto animator = view->getAnimator();
    if (animator) {
      animator->cancel();
    }
  }
  return nullptr;
}

static napi_value UniqueID(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_string_utf8(env, view->id.c_str(), view->id.length(), &result);
  return result;
}

static napi_value NumFrame(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_int64(env, view->numFrames(), &result);
  return result;
}

static napi_value SetStateChangeCallback(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }

  view->setPlayingStateCallback(env, args[0]);
  return nullptr;
}

static napi_value SetProgressUpdateCallback(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }

  view->setProgressCallback(env, args[0]);
  return nullptr;
}

static napi_value RenderScale(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  napi_value result;
  napi_create_double(env, view->renderScale(), &result);
  return result;
}

static napi_value SetRenderScale(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  double value = 1.0f;
  napi_get_value_double(env, args[0], &value);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setRenderScale(value);
  }
  return nullptr;
}

static napi_value CurrentFrame(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  Frame currentFrame = 0;
  if (view != nullptr) {
    currentFrame = view->currentFrame();
  }
  napi_value result;
  napi_create_int64(env, currentFrame, &result);
  return result;
}

static napi_value SetCurrentFrame(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  int64_t currentFrame = 0;
  napi_get_value_int64(env, args[0], &currentFrame);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setCurrentFrame(currentFrame);
  }
  return nullptr;
}

static napi_value SetVisible(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 1;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  if (argc == 0) {
    return nullptr;
  }
  bool visible = false;
  napi_get_value_bool(env, args[0], &visible);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view != nullptr) {
    view->setVisible(visible);
  }
  return nullptr;
}

static napi_value CurrentImage(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  return view->getCurrentPixelMap(env);
}

static napi_value Release(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  JPAGImageView* view = nullptr;
  napi_unwrap(env, jsView, reinterpret_cast<void**>(&view));
  if (view == nullptr) {
    return nullptr;
  }
  view->release(env);
  return nullptr;
}

static void FinalizeJPAGImageView(napi_env, void* finalizeData, void*) {
  auto view = static_cast<JPAGImageView*>(finalizeData);
  XComponentHandler::RemoveListener(view->id);
  std::shared_ptr<JPAGImageView> imageView = nullptr;
  {
    std::lock_guard<std::mutex> autoLock(ViewMapLocker);
    auto result = ViewMap.find(view->id);
    if (result == ViewMap.end()) {
      return;
    }
    imageView = std::move(result->second);
    ViewMap.erase(result);
  }
}

napi_value JPAGImageView::Constructor(napi_env env, napi_callback_info info) {
  napi_value jsView = nullptr;
  size_t argc = 0;
  napi_value args[1] = {0};
  napi_get_cb_info(env, info, &argc, args, &jsView, nullptr);
  std::string id = "PAImageView" + std::to_string(UniqueID::Next());
  auto cView = std::make_shared<JPAGImageView>(id, env);
  cView->_animator = PAGAnimator::MakeFrom(cView);
  XComponentHandler::AddListener(id, cView);
  napi_wrap(env, jsView, cView.get(), FinalizeJPAGImageView, nullptr, nullptr);
  {
    std::lock_guard<std::mutex> autoLock(ViewMapLocker);
    ViewMap.emplace(id, cView);
  }
  return jsView;
}

bool JPAGImageView::Init(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_DEFAULT_METHOD_ENTRY(flush, Flush),
      PAG_DEFAULT_METHOD_ENTRY(setComposition, SetComposition),
      PAG_DEFAULT_METHOD_ENTRY(scaleMode, ScaleMode),
      PAG_DEFAULT_METHOD_ENTRY(setScaleMode, SetScaleMode),
      PAG_DEFAULT_METHOD_ENTRY(matrix, Matrix),
      PAG_DEFAULT_METHOD_ENTRY(setMatrix, SetMatrix),
      PAG_DEFAULT_METHOD_ENTRY(cacheAllFramesInMemory, CacheAllFramesInMemory),
      PAG_DEFAULT_METHOD_ENTRY(setCacheAllFramesInMemory, SetCacheAllFramesInMemory),
      PAG_DEFAULT_METHOD_ENTRY(repeatCount, RepeatCount),
      PAG_DEFAULT_METHOD_ENTRY(setRepeatCount, SetRepeatCount),
      PAG_DEFAULT_METHOD_ENTRY(play, Play),
      PAG_DEFAULT_METHOD_ENTRY(isPlaying, IsPlaying),
      PAG_DEFAULT_METHOD_ENTRY(pause, Pause),
      PAG_DEFAULT_METHOD_ENTRY(setStateChangeCallback, SetStateChangeCallback),
      PAG_DEFAULT_METHOD_ENTRY(setProgressUpdateCallback, SetProgressUpdateCallback),
      PAG_DEFAULT_METHOD_ENTRY(uniqueID, UniqueID),
      PAG_DEFAULT_METHOD_ENTRY(setRenderScale, SetRenderScale),
      PAG_DEFAULT_METHOD_ENTRY(renderScale, RenderScale),
      PAG_DEFAULT_METHOD_ENTRY(currentFrame, CurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(setCurrentFrame, SetCurrentFrame),
      PAG_DEFAULT_METHOD_ENTRY(setVisible, SetVisible),
      PAG_DEFAULT_METHOD_ENTRY(numFrame, NumFrame),
      PAG_DEFAULT_METHOD_ENTRY(currentImage, CurrentImage),
      PAG_DEFAULT_METHOD_ENTRY(update, Update),
      PAG_DEFAULT_METHOD_ENTRY(release, Release)};
  auto status = DefineClass(env, exports, ClassName(), sizeof(classProp) / sizeof(classProp[0]),
                            classProp, Constructor, "");
  if (status != napi_ok) {
    return false;
  }
  return true;
}

JPAGImageView::JPAGImageView(const std::string& id, napi_env env) : id(id) {
  renderSession = std::make_shared<JPAGImageViewRenderSession>();
  eventDispatcher = PAGViewEventDispatcher::Make(env, "PAGImageViewEventDispatcher");
}

void JPAGImageView::onAnimationStart(PAGAnimator*) {
  std::shared_ptr<PAGViewEventDispatcher> dispatcher = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      dispatcher = eventDispatcher;
    }
  }
  if (dispatcher != nullptr) {
    dispatcher->notifyState(PAGAnimatorState::Start);
  }
}

void JPAGImageView::onAnimationCancel(PAGAnimator*) {
  std::shared_ptr<PAGViewEventDispatcher> dispatcher = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      callbackGeneration++;
      dispatcher = eventDispatcher;
    }
  }
  if (dispatcher != nullptr) {
    dispatcher->notifyState(PAGAnimatorState::Cancel);
  }
}

void JPAGImageView::onAnimationEnd(PAGAnimator*) {
  std::shared_ptr<PAGViewEventDispatcher> dispatcher = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      dispatcher = eventDispatcher;
    }
  }
  if (dispatcher != nullptr) {
    dispatcher->notifyState(PAGAnimatorState::End);
  }
}

void JPAGImageView::onAnimationRepeat(PAGAnimator*) {
  std::shared_ptr<PAGViewEventDispatcher> dispatcher = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      dispatcher = eventDispatcher;
    }
  }
  if (dispatcher != nullptr) {
    dispatcher->notifyState(PAGAnimatorState::Repeat);
  }
}

void JPAGImageView::onAnimationUpdate(PAGAnimator* animator) {
  auto progress = animator->progress();
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  std::shared_ptr<PAGViewEventDispatcher> dispatcher = nullptr;
  uint64_t generation = 0;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
      dispatcher = eventDispatcher;
      generation = callbackGeneration;
    }
  }
  if (session == nullptr) {
    return;
  }
  session->updateFrame(progress);
  {
    std::lock_guard lockGuard(locker);
    if (released || renderSession != session || callbackGeneration != generation) {
      dispatcher = nullptr;
    }
  }
  if (dispatcher != nullptr) {
    dispatcher->notifyProgress();
  }
}

void JPAGImageView::onSurfaceCreated(NativeWindow* window) {
  if (OH_NativeWindow_NativeObjectReference(window) != 0) {
    return;
  }
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  std::shared_ptr<PAGAnimator> animator = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
      animator = _animator;
    }
  }
  if (session == nullptr || animator == nullptr) {
    OH_NativeWindow_NativeObjectUnreference(window);
    return;
  }
  session->setWindow(window);
  animator->update();
}

void JPAGImageView::onSurfaceSizeChanged() {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  if (session != nullptr) {
    session->updateSize();
  }
}

void JPAGImageView::onSurfaceDestroyed() {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  if (session != nullptr) {
    session->clearWindow();
  }
}

Frame JPAGImageView::numFrames() {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  return session == nullptr ? 0 : session->numFrames();
}

std::shared_ptr<PAGAnimator> JPAGImageView::getAnimator() {
  std::lock_guard lockGuard(locker);
  return released ? nullptr : _animator;
}

void JPAGImageView::setCurrentFrame(Frame currentFrame) {
  auto animator = getAnimator();
  auto frames = numFrames();
  if (animator != nullptr && frames > 0) {
    animator->setProgress(FrameToProgress(currentFrame, frames));
  }
}

Frame JPAGImageView::currentFrame() {
  auto animator = getAnimator();
  auto frames = numFrames();
  return animator == nullptr || frames <= 0 ? 0 : ProgressToFrame(animator->progress(), frames);
}

void JPAGImageView::setComposition(std::shared_ptr<PAGComposition> composition, float frameRate) {
  auto progress = composition != nullptr ? composition->getProgress() : 0.0;
  auto duration = composition != nullptr ? composition->duration() : 0;
  std::shared_ptr<PAGAnimator> animator = nullptr;
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  bool visible = false;
  {
    std::lock_guard lockGuard(locker);
    if (released) {
      return;
    }
    animator = _animator;
    session = renderSession;
    compositionDuration = duration;
    visible = isVisible;
  }
  if (animator == nullptr || session == nullptr) {
    return;
  }
  session->setComposition(std::move(composition), frameRate);
  animator->setProgress(progress);
  animator->setDuration(visible ? duration : 0);
  if (visible) {
    animator->update();
  }
}

void JPAGImageView::setVisible(bool visible) {
  std::shared_ptr<PAGAnimator> animator = nullptr;
  int64_t duration = 0;
  {
    std::lock_guard lockGuard(locker);
    if (released || isVisible == visible) {
      return;
    }
    isVisible = visible;
    animator = _animator;
    duration = compositionDuration;
  }
  if (animator == nullptr) {
    return;
  }
  animator->setDuration(visible ? duration : 0);
  if (visible) {
    animator->update();
  }
}

void JPAGImageView::setScaleMode(PAGScaleMode scaleMode) {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  if (session != nullptr) {
    session->setScaleMode(scaleMode);
  }
}

PAGScaleMode JPAGImageView::scaleMode() {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  return session == nullptr ? PAGScaleMode::LetterBox : session->getScaleMode();
}

void JPAGImageView::setMatrix(const class Matrix& matrix) {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  if (session != nullptr) {
    session->setMatrix(matrix);
  }
}

class Matrix JPAGImageView::matrix() {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  return session == nullptr ? Matrix::I() : session->getMatrix();
}

void JPAGImageView::setRenderScale(float renderScale) {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  if (session != nullptr) {
    session->setRenderScale(renderScale);
  }
}

float JPAGImageView::renderScale() {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  return session == nullptr ? 1.0f : session->getRenderScale();
}

void JPAGImageView::setCacheAllFramesInMemory(bool cacheAllFramesInMemory) {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  if (session != nullptr) {
    session->setCacheAllFramesInMemory(cacheAllFramesInMemory);
  }
}

bool JPAGImageView::cacheAllFramesInMemory() {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  return session != nullptr && session->getCacheAllFramesInMemory();
}

bool JPAGImageView::flush() {
  auto animator = getAnimator();
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  return animator != nullptr && session != nullptr && session->updateFrame(animator->progress());
}

napi_value JPAGImageView::getCurrentPixelMap(napi_env env) {
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      session = renderSession;
    }
  }
  if (session == nullptr) {
    return nullptr;
  }
  auto bitmap = session->getCurrentBitmap();
  if (bitmap.isEmpty()) {
    return nullptr;
  }
  OhosPixelMapCreateOps ops = {};
  ops.width = bitmap.width();
  ops.height = bitmap.height();
  ops.pixelFormat = bitmap.colorType() == tgfx::ColorType::RGBA_8888 ? PIXEL_FORMAT_RGBA_8888
                                                                     : PIXEL_FORMAT_BGRA_8888;
  ops.alphaType = OHOS_PIXEL_MAP_ALPHA_TYPE_PREMUL;
  ops.editable = false;
  auto pixels = bitmap.lockPixels();
  if (pixels == nullptr) {
    return nullptr;
  }
  napi_value pixelMap = nullptr;
  auto status = OH_PixelMap_CreatePixelMapWithStride(env, ops, pixels, bitmap.byteSize(),
                                                     bitmap.rowBytes(), &pixelMap);
  bitmap.unlockPixels();
  if (status != napi_ok || pixelMap == nullptr) {
    return nullptr;
  }
  auto nativePixelMap = OH_PixelMap_InitNativePixelMap(env, pixelMap);
  if (nativePixelMap == nullptr) {
    return nullptr;
  }
  void* pixelMapAddress = nullptr;
  if (OH_PixelMap_AccessPixels(nativePixelMap, &pixelMapAddress) != IMAGE_RESULT_SUCCESS ||
      pixelMapAddress == nullptr) {
    return nullptr;
  }
  bitmap.readPixels(bitmap.info(), pixelMapAddress);
  OH_PixelMap_UnAccessPixels(nativePixelMap);
  return pixelMap;
}

void JPAGImageView::release(napi_env env) {
  XComponentHandler::RemoveListener(id);
  std::shared_ptr<PAGAnimator> animator = nullptr;
  std::shared_ptr<JPAGImageViewRenderSession> session = nullptr;
  std::shared_ptr<PAGViewEventDispatcher> dispatcher = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (released) {
      return;
    }
    released = true;
    callbackGeneration++;
    isVisible = false;
    compositionDuration = 0;
    animator = std::move(_animator);
    session = std::move(renderSession);
    dispatcher = std::move(eventDispatcher);
  }
  if (dispatcher != nullptr) {
    dispatcher->release(env);
  }
  if (session != nullptr) {
    session->release();
  }
  if (animator != nullptr) {
    tgfx::Task::Run([animator = std::move(animator)]() { animator->cancel(); });
  }
}

void JPAGImageView::setProgressCallback(napi_env env, napi_value callback) {
  std::shared_ptr<PAGViewEventDispatcher> dispatcher = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      dispatcher = eventDispatcher;
    }
  }
  if (dispatcher != nullptr) {
    dispatcher->setProgressCallback(env, callback);
  }
}

void JPAGImageView::setPlayingStateCallback(napi_env env, napi_value callback) {
  std::shared_ptr<PAGViewEventDispatcher> dispatcher = nullptr;
  {
    std::lock_guard lockGuard(locker);
    if (!released) {
      dispatcher = eventDispatcher;
    }
  }
  if (dispatcher != nullptr) {
    dispatcher->setStateCallback(env, callback);
  }
}

}  // namespace pag
