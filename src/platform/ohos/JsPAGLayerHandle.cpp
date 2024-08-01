//
// Created on 2024/7/18.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include "JsPAGLayerHandle.h"
#include <cstdint>
#include "JsHelper.h"
#include "base/utils/Log.h"
#include "pag/file.h"

namespace pag {

static napi_value LayerType(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  napi_value result;
  napi_create_int32(env, static_cast<int>(layer->layerType()), &result);
  return result;
}

static napi_value IsPAGFile(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, layer->isPAGFile(), &result);
  return result;
}

static napi_value LayerName(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  napi_value result;
  auto name = layer->layerName();
  napi_create_string_utf8(env, name.c_str(), name.length(), &result);
  return result;
}

static napi_value Matrix(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  return CreateMatrix(env, layer->matrix());
}

static napi_value SetMatrix(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  layer->setMatrix(GetMatrix(env, args[0]));
  return nullptr;
}

static napi_value ResetMatrix(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  layer->resetMatrix();
  return nullptr;
}

static napi_value GetTotalMatrix(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  return CreateMatrix(env, layer->getTotalMatrix());
}

static napi_value Visible(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  napi_value result;
  if (!layer) {
    napi_get_boolean(env, false, &result);
  } else {
    napi_get_boolean(env, layer->visible(), &result);
  }
  return result;
}

static napi_value SetVisible(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  bool visible = false;
  if (napi_get_value_bool(env, args[0], &visible) != napi_ok) {
    return nullptr;
  }
  layer->setVisible(visible);
  return nullptr;
}

static napi_value EditableIndex(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  napi_value result;
  if (napi_create_int32(env, layer->editableIndex(), &result) != napi_ok) {
    return nullptr;
  }
  return result;
}

static napi_value Parent(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  return JsPAGLayerHandle::ToJs(env, layer->parent());
}

static napi_value Markers(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  return CreateMarkers(env, layer->markers());
}

static napi_value LocalTimeToGlobal(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  int64_t localTime = 0;
  napi_get_value_int64(env, args[0], &localTime);
  napi_value result;
  napi_create_int64(env, layer->localTimeToGlobal(localTime), &result);
  return result;
}

static napi_value GlobalToLocalTime(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  int64_t globalTime = 0;
  napi_get_value_int64(env, args[0], &globalTime);
  napi_value result;
  napi_create_int64(env, layer->globalToLocalTime(globalTime), &result);
  return result;
}

static napi_value Duration(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  napi_value result;
  napi_create_int64(env, layer->duration(), &result);
  return result;
}

static napi_value FrameRate(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  napi_value result;
  napi_create_double(env, layer->frameRate(), &result);
  return result;
}

static napi_value StartTime(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  napi_value result;
  napi_create_int64(env, layer->startTime(), &result);
  return result;
}

static napi_value SetStartTime(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  int64_t startTime = 0;
  napi_get_value_int64(env, args[0], &startTime);
  layer->setStartTime(startTime);
  return nullptr;
}

static napi_value CurrentTime(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  napi_value result;
  napi_create_int64(env, layer->currentTime(), &result);
  return result;
}

static napi_value SetCurrentTime(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  int64_t currentTime = 0;
  napi_get_value_int64(env, args[0], &currentTime);
  layer->setCurrentTime(currentTime);
  return nullptr;
}

static napi_value GetProgress(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }

  napi_value result;
  napi_create_double(env, layer->getProgress(), &result);
  return result;
}

static napi_value SetProgress(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  double progress = 0;
  napi_get_value_double(env, args[0], &progress);
  layer->setProgress(progress);
  return nullptr;
}

static napi_value TrackMatteLayer(napi_env env, napi_callback_info info) {
  size_t argc = 0;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  return JsPAGLayerHandle::ToJs(env, layer->trackMatteLayer());
}

static napi_value GetBounds(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  return CreateRect(env, layer->getBounds());
}

static napi_value ExcludedFromTimeline(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  napi_value result;
  napi_get_boolean(env, layer->excludedFromTimeline(), &result);
  return result;
}

static napi_value SetExcludedFromTimeline(napi_env env, napi_callback_info info) {
  size_t argc = 1;
  napi_value args[1] = {nullptr};
  napi_value jsLayer = nullptr;
  napi_get_cb_info(env, info, &argc, args, &jsLayer, nullptr);
  auto layer = JsPAGLayerHandle::FromJs(env, jsLayer);
  if (!layer) {
    return nullptr;
  }
  bool excludedFromTimeline = 0;
  napi_get_value_bool(env, args[0], &excludedFromTimeline);
  layer->setExcludedFromTimeline(excludedFromTimeline);
  return nullptr;
}

std::shared_ptr<PAGLayer> JsPAGLayerHandle::FromJs(napi_env env, napi_value value) {
  JsPAGLayerHandle* cLayerHandler = nullptr;
  auto status = napi_unwrap(env, value, (void**)&cLayerHandler);
  if (status == napi_ok && cLayerHandler) {
    return cLayerHandler->get();
  } else {
    return nullptr;
  }
}

napi_value JsPAGLayerHandle::ToJs(napi_env env, std::shared_ptr<PAGLayer> layer) {
  if (env == nullptr || layer == nullptr) {
    return nullptr;
  }
  std::string className = "";
  if (layer->isPAGFile()) {
    className = GetFileClassName();
  } else {
    className = GetLayerClassName(layer->layerType());
  }
  auto handler = new JsPAGLayerHandle(layer);
  auto result = NewInstance(env, className, handler);
  if (result == nullptr) {
    delete handler;
  }
  return result;
}

std::string JsPAGLayerHandle::GetLayerClassName(enum LayerType layerType) {
  switch (layerType) {
    case LayerType::PreCompose:
      return "JsPAGComposition";
    case LayerType::Image:
      return "JsPAGImageLayer";
    case LayerType::Text:
      return "JsPAGTextLayer";
    case LayerType::Shape:
      return "JsPAGShapeLayer";
    case LayerType::Solid:
      return "JsPAGSolidLayer";
    default:
      return GetBaseClassName();
  }
}

std::string JsPAGLayerHandle::GetFileClassName() {
  return "JsPAGFile";
}

std::string JsPAGLayerHandle::GetBaseClassName() {
  return "JsPAGLayer";
}

bool JsPAGLayerHandle::InitPAGImageLayerEnv(napi_env, napi_value) {
  return true;
}
bool JsPAGLayerHandle::InitPAGTextLayerEnv(napi_env, napi_value) {
  return true;
}
bool JsPAGLayerHandle::InitPAGShapeLayerEnv(napi_env, napi_value) {
  return true;
}
bool JsPAGLayerHandle::InitPAGSolidLayerEnv(napi_env, napi_value) {
  return true;
}

bool JsPAGLayerHandle::InitPAGLayerEnv(napi_env env, napi_value exports) {
  napi_property_descriptor classProp[] = {
      PAG_DEFAULT_METHOD_ENTRY(layerType, LayerType),
      PAG_DEFAULT_METHOD_ENTRY(layerName, LayerName),
      PAG_DEFAULT_METHOD_ENTRY(isPAGFile, IsPAGFile),
      PAG_DEFAULT_METHOD_ENTRY(matrix, Matrix),
      PAG_DEFAULT_METHOD_ENTRY(setMatrix, SetMatrix),
      PAG_DEFAULT_METHOD_ENTRY(resetMatrix, ResetMatrix),
      PAG_DEFAULT_METHOD_ENTRY(getTotalMatrix, GetTotalMatrix),
      PAG_DEFAULT_METHOD_ENTRY(visible, Visible),
      PAG_DEFAULT_METHOD_ENTRY(setVisible, SetVisible),
      PAG_DEFAULT_METHOD_ENTRY(editableIndex, EditableIndex),
      PAG_DEFAULT_METHOD_ENTRY(parent, Parent),
      PAG_DEFAULT_METHOD_ENTRY(markers, Markers),
      PAG_DEFAULT_METHOD_ENTRY(localTimeToGlobal, LocalTimeToGlobal),
      PAG_DEFAULT_METHOD_ENTRY(globalToLocalTime, GlobalToLocalTime),
      PAG_DEFAULT_METHOD_ENTRY(duration, Duration),
      PAG_DEFAULT_METHOD_ENTRY(frameRate, FrameRate),
      PAG_DEFAULT_METHOD_ENTRY(startTime, StartTime),
      PAG_DEFAULT_METHOD_ENTRY(setStartTime, SetStartTime),
      PAG_DEFAULT_METHOD_ENTRY(currentTime, CurrentTime),
      PAG_DEFAULT_METHOD_ENTRY(setCurrentTime, SetCurrentTime),
      PAG_DEFAULT_METHOD_ENTRY(getProgress, GetProgress),
      PAG_DEFAULT_METHOD_ENTRY(setProgress, SetProgress),
      PAG_DEFAULT_METHOD_ENTRY(trackMatteLayer, TrackMatteLayer),
      PAG_DEFAULT_METHOD_ENTRY(getBounds, GetBounds),
      PAG_DEFAULT_METHOD_ENTRY(excludedFromTimeline, ExcludedFromTimeline),
      PAG_DEFAULT_METHOD_ENTRY(setExcludedFromTimeline, SetExcludedFromTimeline),
  };
  auto status =
      DefineClass(env, exports, GetBaseClassName(), sizeof(classProp) / sizeof(classProp[0]),
                  classProp, ConstructorWithHandler<JsPAGLayerHandle>, "");
  return status == napi_ok;
}

bool JsPAGLayerHandle::Init(napi_env env, napi_value exports) {
  return InitPAGLayerEnv(env, exports) && InitPAGImageLayerEnv(env, exports) &&
         InitPAGTextLayerEnv(env, exports) && InitPAGSolidLayerEnv(env, exports) &&
         InitPAGShapeLayerEnv(env, exports) && InitPAGCompositionLayerEnv(env, exports) &&
         InitPAGFileEnv(env, exports);
}

}  // namespace pag