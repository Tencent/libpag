/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "JNIHelper.h"
#include "JPAGLayerHandle.h"

static jfieldID PAGLayer_nativeContext;

using namespace pag;

void SetPAGLayer(JNIEnv* env, jobject thiz, JPAGLayerHandle* nativeContext) {
  delete reinterpret_cast<JPAGLayerHandle*>(env->GetLongField(thiz, PAGLayer_nativeContext));
  env->SetLongField(thiz, PAGLayer_nativeContext, reinterpret_cast<jlong>(nativeContext));
}

std::shared_ptr<PAGLayer> GetPAGLayer(JNIEnv* env, jobject thiz) {
  auto nativeContext =
      reinterpret_cast<JPAGLayerHandle*>(env->GetLongField(thiz, PAGLayer_nativeContext));
  if (nativeContext == nullptr) {
    return nullptr;
  }
  return nativeContext->get();
}

extern "C" {

PAG_API void Java_org_libpag_PAGLayer_nativeInit(JNIEnv* env, jclass clazz) {
  PAGLayer_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API void Java_org_libpag_PAGLayer_nativeRelease(JNIEnv* env, jobject thiz) {
  SetPAGLayer(env, thiz, nullptr);
}

PAG_API jboolean Java_org_libpag_PAGLayer_nativeEquals(JNIEnv* env, jobject thiz, jobject other) {
  return GetPAGLayer(env, thiz) == GetPAGLayer(env, other);
}

PAG_API jint Java_org_libpag_PAGLayer_layerType(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return static_cast<jint>(LayerType::Unknown);
  }

  return static_cast<jint>(pagLayer->layerType());
}

PAG_API jstring Java_org_libpag_PAGLayer_layerName(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  std::string name = "";
  if (pagLayer != nullptr) {
    name = pagLayer->layerName();
  }
  return SafeConvertToJString(env, name);
}

PAG_API void Java_org_libpag_PAGLayer_matrix(JNIEnv* env, jobject thiz, jfloatArray matrixObject) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }

  auto matrix = pagLayer->matrix();
  auto matrixArray = env->GetFloatArrayElements(matrixObject, nullptr);
  matrix.get9(matrixArray);
  env->ReleaseFloatArrayElements(matrixObject, matrixArray, 0);
}

PAG_API void Java_org_libpag_PAGLayer_setMatrix(JNIEnv* env, jobject thiz,
                                                jfloatArray matrixObject) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }

  jsize length = env->GetArrayLength(matrixObject);
  if (length < 9) {
    return;
  }

  Matrix matrix = {};
  auto matrixArray = env->GetFloatArrayElements(matrixObject, nullptr);
  matrix.set9(matrixArray);
  pagLayer->setMatrix(matrix);
  env->ReleaseFloatArrayElements(matrixObject, matrixArray, 0);
}

PAG_API void Java_org_libpag_PAGLayer_resetMatrix(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }

  pagLayer->resetMatrix();
}

PAG_API void Java_org_libpag_PAGLayer_getTotalMatrix(JNIEnv* env, jobject thiz,
                                                     jfloatArray matrixObject) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }

  Matrix matrix = pagLayer->getTotalMatrix();
  auto matrixArray = env->GetFloatArrayElements(matrixObject, nullptr);
  matrix.get9(matrixArray);
  env->ReleaseFloatArrayElements(matrixObject, matrixArray, 0);
}

PAG_API jboolean Java_org_libpag_PAGLayer_visible(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return JNI_FALSE;
  }

  return static_cast<jboolean>(pagLayer->visible());
}

PAG_API void Java_org_libpag_PAGLayer_setVisible(JNIEnv* env, jobject thiz, jboolean visible) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }

  pagLayer->setVisible(visible);
}

PAG_API jint Java_org_libpag_PAGLayer_editableIndex(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return -1;
  }

  return pagLayer->editableIndex();
}

PAG_API jobject Java_org_libpag_PAGLayer_parent(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return nullptr;
  }

  std::shared_ptr<PAGComposition> composition = pagLayer->parent();
  if (composition == nullptr) {
    return nullptr;
  }

  return ToPAGLayerJavaObject(env, composition);
}

PAG_API jobjectArray Java_org_libpag_PAGLayer_markers(JNIEnv* env, jobject thiz) {
  static Global<jclass> PAGMarker_Class = env->FindClass("org/libpag/PAGMarker");
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr || pagLayer->markers().empty()) {
    return env->NewObjectArray(0, PAGMarker_Class.get(), nullptr);
  }
  int markerSize = pagLayer->markers().size();
  jobjectArray markerArray = env->NewObjectArray(markerSize, PAGMarker_Class.get(), nullptr);
  for (int i = 0; i < markerSize; ++i) {
    auto jMarker = ToPAGMarkerObject(env, pagLayer->markers()[i]);
    env->SetObjectArrayElement(markerArray, i, jMarker);
  }
  return markerArray;
}

PAG_API jlong Java_org_libpag_PAGLayer_localTimeToGlobal(JNIEnv* env, jobject thiz, jlong time) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return time;
  }
  return pagLayer->localTimeToGlobal(time);
}

PAG_API jlong Java_org_libpag_PAGLayer_globalToLocalTime(JNIEnv* env, jobject thiz, jlong time) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return time;
  }
  return pagLayer->globalToLocalTime(time);
}

PAG_API jlong Java_org_libpag_PAGLayer_duration(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 0;
  }
  return pagLayer->duration();
}

PAG_API jfloat Java_org_libpag_PAGLayer_frameRate(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 60;
  }
  return pagLayer->frameRate();
}

PAG_API jlong Java_org_libpag_PAGLayer_startTime(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 0;
  }
  return pagLayer->startTime();
}

PAG_API void Java_org_libpag_PAGLayer_setStartTime(JNIEnv* env, jobject thiz, jlong time) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  pagLayer->setStartTime(time);
}

PAG_API jlong Java_org_libpag_PAGLayer_currentTime(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 0;
  }
  return pagLayer->currentTime();
}

PAG_API void Java_org_libpag_PAGLayer_setCurrentTime(JNIEnv* env, jobject thiz, jlong time) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  pagLayer->setCurrentTime(time);
}

PAG_API jdouble Java_org_libpag_PAGLayer_getProgress(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 0;
  }
  return pagLayer->getProgress();
}

PAG_API void Java_org_libpag_PAGLayer_setProgress(JNIEnv* env, jobject thiz, jdouble progress) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }
  pagLayer->setProgress(progress);
}

PAG_API jobject Java_org_libpag_PAGLayer_trackMatteLayer(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return nullptr;
  }
  if (pagLayer->trackMatteLayer() == nullptr) {
    return nullptr;
  }
  return ToPAGLayerJavaObject(env, pagLayer->trackMatteLayer());
}

PAG_API jobject Java_org_libpag_PAGLayer_getBounds(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return MakeRectFObject(env, 0.0f, 0.0f, 0.0f, 0.0f);
  }

  auto rect = pagLayer->getBounds();
  return MakeRectFObject(env, rect.x(), rect.y(), rect.width(), rect.height());
}

PAG_API jboolean Java_org_libpag_PAGLayer_excludedFromTimeline(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return JNI_FALSE;
  }

  return static_cast<jboolean>(pagLayer->excludedFromTimeline());
}

PAG_API void Java_org_libpag_PAGLayer_setExcludedFromTimeline(JNIEnv* env, jobject thiz,
                                                              jboolean value) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }

  pagLayer->setExcludedFromTimeline(value);
}

PAG_API jfloat Java_org_libpag_PAGLayer_alpha(JNIEnv* env, jobject thiz) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return 0.0f;
  }

  return pagLayer->alpha();
}

PAG_API void Java_org_libpag_PAGLayer_setAlpha(JNIEnv* env, jobject thiz, jfloat value) {
  auto pagLayer = GetPAGLayer(env, thiz);
  if (pagLayer == nullptr) {
    return;
  }

  pagLayer->setAlpha(value);
}
}
