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

static jfieldID PAGComposition_nativeContext;

using namespace pag;

std::shared_ptr<PAGComposition> GetPAGComposition(JNIEnv* env, jobject thiz) {
  auto nativeContext =
      reinterpret_cast<JPAGLayerHandle*>(env->GetLongField(thiz, PAGComposition_nativeContext));
  if (nativeContext == nullptr) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGComposition>(nativeContext->get());
}

extern "C" {

PAG_API void Java_org_libpag_PAGComposition_nativeInit(JNIEnv* env, jclass clazz) {
  PAGComposition_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API jobject Java_org_libpag_PAGComposition_Make(JNIEnv* env, jclass, jint width, jint height) {
  auto composition = PAGComposition::Make(width, height);
  if (composition == nullptr) {
    return nullptr;
  }
  return ToPAGLayerJavaObject(env, composition);
}

PAG_API jint Java_org_libpag_PAGComposition_width(JNIEnv* env, jobject thiz) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return 0;
  }
  return composition->width();
}

PAG_API jint Java_org_libpag_PAGComposition_height(JNIEnv* env, jobject thiz) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return 0;
  }
  return composition->height();
}

PAG_API void Java_org_libpag_PAGComposition_setContentSize(JNIEnv* env, jobject thiz, jint width,
                                                           jint height) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return;
  }
  composition->setContentSize(width, height);
}

PAG_API jint Java_org_libpag_PAGComposition_numChildren(JNIEnv* env, jobject thiz) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return 0;
  }

  return composition->numChildren();
}

PAG_API jobject Java_org_libpag_PAGComposition_getLayerAt(JNIEnv* env, jobject thiz, jint index) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return nullptr;
  }

  auto pagLayer = composition->getLayerAt(index);
  if (pagLayer == nullptr) {
    return nullptr;
  }

  return ToPAGLayerJavaObject(env, pagLayer);
}

PAG_API jint Java_org_libpag_PAGComposition_getLayerIndex(JNIEnv* env, jobject thiz,
                                                          jobject layer) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return -1;
  }

  auto pagLayer = ToPAGLayerNativeObject(env, layer);
  if (pagLayer == nullptr) {
    return -1;
  }

  return composition->getLayerIndex(pagLayer);
}

PAG_API void Java_org_libpag_PAGComposition_setLayerIndex(JNIEnv* env, jobject thiz, jobject layer,
                                                          jint index) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return;
  }

  auto pagLayer = ToPAGLayerNativeObject(env, layer);
  if (pagLayer == nullptr) {
    return;
  }

  composition->setLayerIndex(pagLayer, index);
}

PAG_API void Java_org_libpag_PAGComposition_addLayer(JNIEnv* env, jobject thiz, jobject layer) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return;
  }
  auto pagLayer = ToPAGLayerNativeObject(env, layer);
  if (pagLayer == nullptr) {
    return;
  }
  composition->addLayer(pagLayer);
}

PAG_API void Java_org_libpag_PAGComposition_addLayerAt(JNIEnv* env, jobject thiz, jobject layer,
                                                       jint index) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return;
  }
  auto pagLayer = ToPAGLayerNativeObject(env, layer);
  if (pagLayer == nullptr) {
    return;
  }
  composition->addLayerAt(pagLayer, index);
}

PAG_API jboolean Java_org_libpag_PAGComposition_contains(JNIEnv* env, jobject thiz, jobject layer) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return JNI_FALSE;
  }
  auto pagLayer = ToPAGLayerNativeObject(env, layer);
  if (pagLayer == nullptr) {
    return JNI_FALSE;
  }
  return (jboolean)composition->contains(pagLayer);
}

PAG_API jobject Java_org_libpag_PAGComposition_removeLayer(JNIEnv* env, jobject thiz,
                                                           jobject layer) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return nullptr;
  }
  auto pagLayer = ToPAGLayerNativeObject(env, layer);
  if (pagLayer == nullptr) {
    return nullptr;
  }
  return ToPAGLayerJavaObject(env, composition->removeLayer(pagLayer));
}

PAG_API jobject Java_org_libpag_PAGComposition_removeLayerAt(JNIEnv* env, jobject thiz,
                                                             jint index) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return nullptr;
  }
  return ToPAGLayerJavaObject(env, composition->removeLayerAt(index));
}

PAG_API void Java_org_libpag_PAGComposition_removeAllLayers(JNIEnv* env, jobject thiz) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return;
  }
  composition->removeAllLayers();
}

PAG_API void Java_org_libpag_PAGComposition_swapLayer(JNIEnv* env, jobject thiz, jobject layer1,
                                                      jobject layer2) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return;
  }
  auto pagLayer1 = ToPAGLayerNativeObject(env, layer1);
  if (pagLayer1 == nullptr) {
    return;
  }
  auto pagLayer2 = ToPAGLayerNativeObject(env, layer2);
  if (pagLayer1 == nullptr) {
    return;
  }
  composition->swapLayer(pagLayer1, pagLayer2);
}

PAG_API void Java_org_libpag_PAGComposition_swapLayerAt(JNIEnv* env, jobject thiz, jint index1,
                                                        jint index2) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return;
  }
  composition->swapLayerAt(index1, index2);
}

PAG_API jobject Java_org_libpag_PAGComposition_audioBytes(JNIEnv* env, jobject thiz) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr || composition->audioBytes() == nullptr) {
    return nullptr;
  }
  return env->NewDirectByteBuffer(composition->audioBytes()->data(),
                                  composition->audioBytes()->length());
}

PAG_API jlong Java_org_libpag_PAGComposition_audioStartTime(JNIEnv* env, jobject thiz) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return 0;
  }

  return composition->audioStartTime();
}

PAG_API jobjectArray Java_org_libpag_PAGComposition_audioMarkers(JNIEnv* env, jobject thiz) {
  static Global<jclass> PAGMarker_Class = env->FindClass("org/libpag/PAGMarker");
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr || composition->audioMarkers().empty()) {
    return env->NewObjectArray(0, PAGMarker_Class.get(), nullptr);
  }
  int markerSize = composition->audioMarkers().size();
  jobjectArray markerArray = env->NewObjectArray(markerSize, PAGMarker_Class.get(), nullptr);
  for (int i = 0; i < markerSize; ++i) {
    auto jMarker = ToPAGMarkerObject(env, composition->audioMarkers()[i]);
    env->SetObjectArrayElement(markerArray, i, jMarker);
  }
  return markerArray;
}

PAG_API jobjectArray Java_org_libpag_PAGComposition_getLayersByName(JNIEnv* env, jobject thiz,
                                                                    jstring layerName) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return ToPAGLayerJavaObjectList(env, {});
  }
  auto name = SafeConvertToStdString(env, layerName);
  auto layers = composition->getLayersByName(name);
  return ToPAGLayerJavaObjectList(env, layers);
}

PAG_API jobjectArray Java_org_libpag_PAGComposition_getLayersUnderPoint(JNIEnv* env, jobject thiz,
                                                                        jfloat localX,
                                                                        jfloat localY) {
  auto composition = GetPAGComposition(env, thiz);
  if (composition == nullptr) {
    return ToPAGLayerJavaObjectList(env, {});
  }
  auto layers = composition->getLayersUnderPoint(localX, localY);
  return ToPAGLayerJavaObjectList(env, layers);
}
}
