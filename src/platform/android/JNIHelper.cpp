/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/bitmap.h>
#include <cassert>
#include <string>
#include "JPAGLayerHandle.h"

static constexpr int BITMAP_FLAGS_ALPHA_UNPREMUL = 2;
static constexpr int BITMAP_FLAGS_IS_HARDWARE = 1 << 31;

jobject MakeRectFObject(JNIEnv* env, float x, float y, float width, float height) {
  static Global<jclass> RectFClass(env, env->FindClass("android/graphics/RectF"));
  static auto RectFConstructID = env->GetMethodID(RectFClass.get(), "<init>", "(FFFF)V");
  return env->NewObject(RectFClass.get(), RectFConstructID, x, y, x + width, y + height);
}

jint MakeColorInt(JNIEnv*, uint32_t red, uint32_t green, uint32_t blue) {
  uint32_t color = (255 << 24) | (red << 16) | (green << 8) | (blue << 0);
  return static_cast<int>(color);
}

jobject MakePAGFontObject(JNIEnv* env, const char* familyName, const char* familyStyle) {
  static Global<jclass> PAGFontClass(env, env->FindClass("org/libpag/PAGFont"));
  static jmethodID PAGFontConstructID = env->GetMethodID(PAGFontClass.get(), "<init>", "()V");
  static jfieldID PAGFont_fontFamily =
      env->GetFieldID(PAGFontClass.get(), "fontFamily", "Ljava/lang/String;");
  static jfieldID PAGFont_fontStyle =
      env->GetFieldID(PAGFontClass.get(), "fontStyle", "Ljava/lang/String;");

  jobject fontObject = env->NewObject(PAGFontClass.get(), PAGFontConstructID);
  Local<jstring> fontFamily = {env, SafeConvertToJString(env, familyName)};
  env->SetObjectField(fontObject, PAGFont_fontFamily, fontFamily.get());
  Local<jstring> fontStyle = {env, SafeConvertToJString(env, familyStyle)};
  env->SetObjectField(fontObject, PAGFont_fontStyle, fontStyle.get());
  return fontObject;
}

jobject MakeByteBufferObject(JNIEnv* env, const void* bytes, size_t length) {
  static Global<jclass> ByteBufferClass(env, env->FindClass("java/nio/ByteBuffer"));
  static jmethodID ByteBuffer_wrap =
      env->GetStaticMethodID(ByteBufferClass.get(), "wrap", "([B)Ljava/nio/ByteBuffer;");
  Local<jbyteArray> byteArray = {env, env->NewByteArray(length)};
  env->SetByteArrayRegion(byteArray.get(), 0, length, (jbyte*)bytes);
  return env->CallStaticObjectMethod(ByteBufferClass.get(), ByteBuffer_wrap, byteArray.get());
}

pag::Color ToColor(JNIEnv*, jint value) {
  auto color = static_cast<uint32_t>(value);
  auto red = (((color) >> 16) & 0xFF);
  auto green = (((color) >> 8) & 0xFF);
  auto blue = (((color) >> 0) & 0xFF);
  return {static_cast<uint8_t>(red), static_cast<uint8_t>(green), static_cast<uint8_t>(blue)};
}

std::unique_ptr<pag::ByteData> ReadBytesFromAssets(JNIEnv* env, jobject managerObj,
                                                   jstring pathObj) {
  if (managerObj == nullptr || pathObj == nullptr) {
    return nullptr;
  }
  auto manager = AAssetManager_fromJava(env, managerObj);
  if (manager == nullptr) {
    return nullptr;
  }
  auto path = SafeConvertToStdString(env, pathObj);
  if (path.empty()) {
    return nullptr;
  }
  auto asset = AAssetManager_open(manager, path.c_str(), AASSET_MODE_UNKNOWN);
  if (asset == nullptr) {
    return nullptr;
  }
  auto size = static_cast<size_t>(AAsset_getLength(asset));
  auto byteData = pag::ByteData::Make(size);
  auto numBytes = AAsset_read(asset, byteData->data(), size);
  AAsset_close(asset);
  if (numBytes <= 0) {
    return nullptr;
  }
  return byteData;
}

RectData ToRectData(JNIEnv* env, jobject rect) {
  static Global<jclass> RectFClass(env, env->FindClass("android/graphics/RectF"));
  static auto leftID = env->GetFieldID(RectFClass.get(), "left", "F");
  static auto topID = env->GetFieldID(RectFClass.get(), "top", "F");
  static auto rightID = env->GetFieldID(RectFClass.get(), "right", "F");
  static auto bottomID = env->GetFieldID(RectFClass.get(), "bottom", "F");
  auto left = env->GetFloatField(rect, leftID);
  auto top = env->GetFloatField(rect, topID);
  auto right = env->GetFloatField(rect, rightID);
  auto bottom = env->GetFloatField(rect, bottomID);
  return {left, top, right - left, bottom - top};
}

jobjectArray ToPAGLayerJavaObjectList(JNIEnv* env,
                                      const std::vector<std::shared_ptr<pag::PAGLayer>>& layers) {
  static Global<jclass> PAGLayer_Class(env, env->FindClass("org/libpag/PAGLayer"));
  if (layers.empty()) {
    return env->NewObjectArray(0, PAGLayer_Class.get(), nullptr);
  }
  jobjectArray layerArray = env->NewObjectArray(layers.size(), PAGLayer_Class.get(), nullptr);
  for (size_t i = 0; i < layers.size(); ++i) {
    auto layer = layers[i];
    Local<jobject> jLayer = {env, ToPAGLayerJavaObject(env, layer)};
    env->SetObjectArrayElement(layerArray, i, jLayer.get());
  }
  return layerArray;
}

jobject ToPAGLayerJavaObject(JNIEnv* env, std::shared_ptr<pag::PAGLayer> pagLayer) {
  if (env == nullptr || pagLayer == nullptr) {
    return nullptr;
  }
  jobject layerObject = nullptr;
  switch (pagLayer->layerType()) {
    case pag::LayerType::Shape: {
      static auto PAGLayer_Class = Global<jclass>(env, env->FindClass("org/libpag/PAGShapeLayer"));
      static auto PAGLayer_Constructor = env->GetMethodID(PAGLayer_Class.get(), "<init>", "(J)V");
      layerObject = env->NewObject(PAGLayer_Class.get(), PAGLayer_Constructor,
                                   reinterpret_cast<jlong>(new JPAGLayerHandle(pagLayer)));
      break;
    }
    case pag::LayerType::Solid: {
      static auto PAGLayer_Class = Global<jclass>(env, env->FindClass("org/libpag/PAGSolidLayer"));
      static auto PAGLayer_Constructor = env->GetMethodID(PAGLayer_Class.get(), "<init>", "(J)V");
      layerObject = env->NewObject(PAGLayer_Class.get(), PAGLayer_Constructor,
                                   reinterpret_cast<jlong>(new JPAGLayerHandle(pagLayer)));
      break;
    }
    case pag::LayerType::PreCompose: {
      if (std::static_pointer_cast<pag::PAGComposition>(pagLayer)->isPAGFile()) {
        static auto PAGLayer_Class = Global<jclass>(env, env->FindClass("org/libpag/PAGFile"));
        static auto PAGLayer_Constructor = env->GetMethodID(PAGLayer_Class.get(), "<init>", "(J)V");
        layerObject = env->NewObject(PAGLayer_Class.get(), PAGLayer_Constructor,
                                     reinterpret_cast<jlong>(new JPAGLayerHandle(pagLayer)));
      } else {
        static auto PAGLayer_Class =
            Global<jclass>(env, env->FindClass("org/libpag/PAGComposition"));
        static auto PAGLayer_Constructor = env->GetMethodID(PAGLayer_Class.get(), "<init>", "(J)V");
        layerObject = env->NewObject(PAGLayer_Class.get(), PAGLayer_Constructor,
                                     reinterpret_cast<jlong>(new JPAGLayerHandle(pagLayer)));
      }

      break;
    }
    case pag::LayerType::Text: {
      static auto PAGLayer_Class = Global<jclass>(env, env->FindClass("org/libpag/PAGTextLayer"));
      static auto PAGLayer_Constructor = env->GetMethodID(PAGLayer_Class.get(), "<init>", "(J)V");
      layerObject = env->NewObject(PAGLayer_Class.get(), PAGLayer_Constructor,
                                   reinterpret_cast<jlong>(new JPAGLayerHandle(pagLayer)));
      break;
    }
    case pag::LayerType::Image: {
      static auto PAGLayer_Class = Global<jclass>(env, env->FindClass("org/libpag/PAGImageLayer"));
      static auto PAGLayer_Constructor = env->GetMethodID(PAGLayer_Class.get(), "<init>", "(J)V");
      layerObject = env->NewObject(PAGLayer_Class.get(), PAGLayer_Constructor,
                                   reinterpret_cast<jlong>(new JPAGLayerHandle(pagLayer)));
      break;
    }
    default: {
      static auto PAGLayer_Class = Global<jclass>(env, env->FindClass("org/libpag/PAGLayer"));
      static auto PAGLayer_Constructor = env->GetMethodID(PAGLayer_Class.get(), "<init>", "(J)V");
      layerObject = env->NewObject(PAGLayer_Class.get(), PAGLayer_Constructor,
                                   reinterpret_cast<jlong>(new JPAGLayerHandle(pagLayer)));
      break;
    }
  }
  return layerObject;
}

std::shared_ptr<pag::PAGLayer> ToPAGLayerNativeObject(JNIEnv* env, jobject jLayer) {
  if (env == nullptr || jLayer == nullptr) {
    return nullptr;
  }

  static Global<jclass> PAGLayer_Class(env, env->FindClass("org/libpag/PAGLayer"));
  static auto PAGLayer_nativeContext = env->GetFieldID(PAGLayer_Class.get(), "nativeContext", "J");

  auto nativeContext =
      reinterpret_cast<JPAGLayerHandle*>(env->GetLongField(jLayer, PAGLayer_nativeContext));
  if (nativeContext == nullptr) {
    return nullptr;
  }

  return nativeContext->get();
}

std::shared_ptr<pag::PAGComposition> ToPAGCompositionNativeObject(JNIEnv* env,
                                                                  jobject jComposition) {
  if (env == nullptr || jComposition == nullptr) {
    return nullptr;
  }

  static Global<jclass> PAGComposition_Class(env, env->FindClass("org/libpag/PAGComposition"));
  static auto PAGComposition_nativeContext =
      env->GetFieldID(PAGComposition_Class.get(), "nativeContext", "J");

  auto nativeContext = reinterpret_cast<JPAGLayerHandle*>(
      env->GetLongField(jComposition, PAGComposition_nativeContext));
  if (nativeContext == nullptr) {
    return nullptr;
  }

  return std::static_pointer_cast<pag::PAGComposition>(nativeContext->get());
}

jobject ToPAGMarkerObject(JNIEnv* env, const pag::Marker* marker) {
  if (env == nullptr || marker == nullptr) {
    return nullptr;
  }

  static Global<jclass> PAGMarker_Class(env, env->FindClass("org/libpag/PAGMarker"));
  static auto PAGMarker_Construct =
      env->GetMethodID(PAGMarker_Class.get(), "<init>", "(JJLjava/lang/String;)V");
  Local<jstring> comment = {env, SafeConvertToJString(env, marker->comment.c_str())};
  return env->NewObject(PAGMarker_Class.get(), PAGMarker_Construct, marker->startTime,
                        marker->duration, comment.get());
}

jobject ToPAGVideoRangeObject(JNIEnv* env, const pag::PAGVideoRange& range) {
  if (env == nullptr) {
    return nullptr;
  }

  static Global<jclass> PAGVideoRange_Class(env, env->FindClass("org/libpag/PAGVideoRange"));
  static auto PAGVideoRange_Construct =
      env->GetMethodID(PAGVideoRange_Class.get(), "<init>", "(JJJZ)V");
  return env->NewObject(PAGVideoRange_Class.get(), PAGVideoRange_Construct, range.startTime(),
                        range.endTime(), range.playDuration(), range.reversed());
}

tgfx::ImageInfo GetImageInfo(JNIEnv* env, jobject bitmap) {
  AndroidBitmapInfo bitmapInfo = {};
  if (bitmap == nullptr || AndroidBitmap_getInfo(env, bitmap, &bitmapInfo) != 0 ||
      (bitmapInfo.flags & BITMAP_FLAGS_IS_HARDWARE)) {
    return {};
  }
  tgfx::AlphaType alphaType = (bitmapInfo.flags & BITMAP_FLAGS_ALPHA_UNPREMUL)
                                  ? tgfx::AlphaType::Unpremultiplied
                                  : tgfx::AlphaType::Premultiplied;
  tgfx::ColorType colorType;
  switch (bitmapInfo.format) {
    case ANDROID_BITMAP_FORMAT_RGBA_8888:
      colorType = tgfx::ColorType::RGBA_8888;
      break;
    case ANDROID_BITMAP_FORMAT_A_8:
      colorType = tgfx::ColorType::ALPHA_8;
      break;
    default:
      colorType = tgfx::ColorType::Unknown;
      break;
  }
  return tgfx::ImageInfo::Make(bitmapInfo.width, bitmapInfo.height, colorType, alphaType,
                               bitmapInfo.stride);
}