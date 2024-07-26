#include "napi/native_api.h"
#include <ace/xcomponent/native_interface_xcomponent.h>
#include "tgfx/opengl/egl/EGLWindow.h"
#include "pag/pag.h"
#include "drawers/AppHost.h"
#include "drawers/Drawer.h"

#include "src/platform/ohos/GPUDrawable.h"

static float screenDensity = 1.0f;
static std::shared_ptr<drawers::AppHost> appHost = nullptr;
static std::shared_ptr<tgfx::Window> window = nullptr;

static std::shared_ptr<drawers::AppHost> CreateAppHost();

static pag::PAGPlayer* pagPlayer = nullptr;
static std::shared_ptr<pag::PAGSurface> pagSurface = nullptr;


static napi_value OnUpdateDensity(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  double value;
  napi_get_value_double(env, args[0], &value);
  screenDensity = static_cast<float>(value);
  return nullptr;
}

static napi_value AddImageFromEncoded(napi_env env, napi_callback_info info) {
  size_t argc = 2;
  napi_value args[2] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  size_t strSize;
  char srcBuf[2048];
  napi_get_value_string_utf8(env, args[0], srcBuf, sizeof(srcBuf), &strSize);
  std::string name = srcBuf;
  size_t length;
  void* data;
  auto code = napi_get_arraybuffer_info(env, args[1], &data, &length);
  if (code != napi_ok) {
    return nullptr;
  }
  auto tgfxData = tgfx::Data::MakeWithCopy(data, length);
  if (appHost == nullptr) {
    appHost = CreateAppHost();
  }
  appHost->addImage(name, tgfx::Image::MakeFromEncoded(tgfxData));
  return nullptr;
}

static void Draw(int index) {
//   if (window == nullptr || appHost == nullptr || appHost->width() <= 0 || appHost->height() <= 0) {
//     return;
//   }
//   auto device = window->getDevice();
//   auto context = device->lockContext();
//   if (context == nullptr) {
//     printf("Fail to lock context from the Device.\n");
//     return;
//   }
//   auto surface = window->getSurface(context);
//   if (surface == nullptr) {
//     device->unlock();
//     return;
//   }
//   auto canvas = surface->getCanvas();
//   canvas->clear();
//   canvas->save();
//   auto numDrawers = drawers::Drawer::Count() - 1;
//   index = (index % numDrawers) + 1;
//   auto drawer = drawers::Drawer::GetByName("GridBackground");
//   drawer->draw(canvas, appHost.get());
//   drawer = drawers::Drawer::GetByIndex(index);
//   drawer->draw(canvas, appHost.get());
//   canvas->restore();
//   surface->flush();
//   context->submit();
//   window->present(context);
//   device->unlock();
    if (index) {
        
    }
    
  auto device = window->getDevice();
  auto context = device->lockContext();
  if (context == nullptr) {
    printf("Fail to lock context from the Device.\n");
    return;
  }
  auto surface = window->getSurface(context);
  if (surface == nullptr) {
    device->unlock();
    return;
  }
  auto canvas = surface->getCanvas();
  canvas->clear();
  canvas->save();
  auto numDrawers = drawers::Drawer::Count() - 1;
  index = (index % numDrawers) + 1;
  auto drawer = drawers::Drawer::GetByName("GridBackground");
  drawer->draw(canvas, appHost.get());
  drawer = drawers::Drawer::GetByIndex(index);
  drawer->draw(canvas, appHost.get());
  canvas->restore();
  surface->flush();
  context->submit();
  window->present(context);
  device->unlock();
    
    
}

static napi_value OnDraw(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
  double value;
  napi_get_value_double(env, args[0], &value);
  Draw(static_cast<int>(value));
  return nullptr;
}

static std::shared_ptr<drawers::AppHost> CreateAppHost() {
  auto appHost = std::make_shared<drawers::AppHost>();
  static const std::string FallbackFontFileNames[] = {"/system/fonts/HarmonyOS_Sans.ttf",
                                                      "/system/fonts/HarmonyOS_Sans_SC.ttf",
                                                      "/system/fonts/HarmonyOS_Sans_TC.ttf"};
  for (auto& fileName : FallbackFontFileNames) {
    auto typeface = tgfx::Typeface::MakeFromPath(fileName);
    if (typeface != nullptr) {
      appHost->addTypeface("default", std::move(typeface));
      break;
    }
  }
  auto typeface = tgfx::Typeface::MakeFromPath("/system/fonts/HMOSColorEmojiCompat.ttf");
  if (typeface != nullptr) {
    appHost->addTypeface("emoji", std::move(typeface));
  }
  return appHost;
}

static void UpdateSize(OH_NativeXComponent* component, void* nativeWindow) {
  uint64_t width;
  uint64_t height;
  int32_t ret = OH_NativeXComponent_GetXComponentSize(component, nativeWindow, &width, &height);
  if (ret != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
    return;
  }
  if (appHost == nullptr) {
    appHost = CreateAppHost();
  }
  appHost->updateScreen(static_cast<int>(width), static_cast<int>(height), screenDensity);
  if (window != nullptr) {
    window->invalidSize();
  }
}

static void OnSurfaceChangedCB(OH_NativeXComponent* component, void* nativeWindow) {
  UpdateSize(component, nativeWindow);
    if (component && nativeWindow) {
        
    }
    pagSurface->updateSize();
}

static void OnSurfaceDestroyedCB(OH_NativeXComponent*, void*) {
    window = nullptr;
    delete pagPlayer;
}

static void DispatchTouchEventCB(OH_NativeXComponent*, void*) {
}

static void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* nativeWindow) {
  UpdateSize(component, nativeWindow);
//   window = tgfx::EGLWindow::MakeFrom(reinterpret_cast<EGLNativeWindowType>(nativeWindow));
//   if (window == nullptr) {
//     printf("OnSurfaceCreatedCB() Invalid surface specified.\n");
//     return;
//   }
//   Draw(0);
    if (component && nativeWindow) {
        
    }
    std::string filePath = "/data/storage/el1/bundle/hello2d/resources/resfile/data_video.pag";
    auto pagFile = pag::PAGFile::Load(filePath);
    if (pagFile == nullptr) {
        return;
    }
    
    auto drawable = pag::GPUDrawable::FromWindow(reinterpret_cast<NativeWindow*>(nativeWindow));
    if (drawable == nullptr) {
        return;
    }
    pagSurface = pag::PAGSurface::MakeFrom(std::static_pointer_cast<pag::Drawable>(drawable));
    if (pagSurface == nullptr) {
        return;
    }
    pagPlayer = new pag::PAGPlayer();
    pagPlayer->setComposition(pagFile);
    pagPlayer->setSurface(pagSurface);
    pagPlayer->setProgress(0.5);
    bool status = pagPlayer->flush();
    if (status) {
        
    }
    //Draw(0);
    
}

static void RegisterCallback(napi_env env, napi_value exports) {
  napi_status status;
  napi_value exportInstance = nullptr;
  OH_NativeXComponent* nativeXComponent = nullptr;
  status = napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance);
  if (status != napi_ok) {
    return;
  }
  status = napi_unwrap(env, exportInstance, reinterpret_cast<void**>(&nativeXComponent));
  if (status != napi_ok) {
    return;
  }
  static OH_NativeXComponent_Callback callback;
  callback.OnSurfaceCreated = OnSurfaceCreatedCB;
  callback.OnSurfaceChanged = OnSurfaceChangedCB;
  callback.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
  callback.DispatchTouchEvent = DispatchTouchEventCB;
  OH_NativeXComponent_RegisterCallback(nativeXComponent, &callback);
}

EXTERN_C_START
static napi_value Init(napi_env env, napi_value exports) {
  napi_property_descriptor desc[] = {
      {"draw", nullptr, OnDraw, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"updateDensity", nullptr, OnUpdateDensity, nullptr, nullptr, nullptr, napi_default, nullptr},
      {"addImageFromEncoded", nullptr, AddImageFromEncoded, nullptr, nullptr, nullptr, napi_default,
       nullptr}};
  napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
  RegisterCallback(env, exports);
  return exports;
}
EXTERN_C_END

static napi_module demoModule = {1, 0, nullptr, Init, "hello2d", ((void*)0), {0}};

extern "C" __attribute__((constructor)) void RegisterHello2dModule(void) {
  napi_module_register(&demoModule);
}