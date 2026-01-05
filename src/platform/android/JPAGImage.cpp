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

#include "JPAGImage.h"
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <cassert>
#include "JNIHelper.h"
#include "NativePlatform.h"
#include "base/utils/TGFXCast.h"
#include "rendering/editing/StillImage.h"

namespace pag {
static jfieldID PAGImage_nativeContext;
};

using namespace pag;

std::shared_ptr<PAGImage> getPAGImage(JNIEnv* env, jobject thiz) {
  auto pagImage = reinterpret_cast<JPAGImage*>(env->GetLongField(thiz, PAGImage_nativeContext));
  if (pagImage == nullptr) {
    return nullptr;
  }
  return pagImage->get();
}

void setPAGImage(JNIEnv* env, jobject thiz, JPAGImage* pagImage) {
  auto old = reinterpret_cast<JPAGImage*>(env->GetLongField(thiz, PAGImage_nativeContext));
  if (old != nullptr) {
    delete old;
  }
  env->SetLongField(thiz, PAGImage_nativeContext, (jlong)pagImage);
}

extern "C" {

PAG_API void Java_org_libpag_PAGImage_nativeRelease(JNIEnv* env, jobject thiz) {
  auto jPagImage = reinterpret_cast<JPAGImage*>(env->GetLongField(thiz, PAGImage_nativeContext));
  if (jPagImage != nullptr) {
    jPagImage->clear();
  }
}

PAG_API void Java_org_libpag_PAGImage_nativeFinalize(JNIEnv* env, jobject thiz) {
  setPAGImage(env, thiz, nullptr);
}

PAG_API void Java_org_libpag_PAGImage_nativeInit(JNIEnv* env, jclass clazz) {
  PAGImage_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API jlong Java_org_libpag_PAGImage_LoadFromBitmap(JNIEnv*, jclass, jobject bitmap) {
  auto image = tgfx::Image::MakeFrom(bitmap);
  auto pagImage = StillImage::MakeFrom(std::move(image));
  if (pagImage == nullptr) {
    LOGE("PAGImage.LoadFromBitmap() Invalid bitmap specified.");
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGImage(pagImage));
}

PAG_API jlong Java_org_libpag_PAGImage_LoadFromPath(JNIEnv* env, jclass, jstring pathObj) {
  if (pathObj == nullptr) {
    LOGE("PAGImage.LoadFromPath() Invalid path specified.");
    return 0;
  }
  auto path = SafeConvertToStdString(env, pathObj);
  if (path.empty()) {
    return 0;
  }
  auto pagImage = PAGImage::FromPath(path);
  if (pagImage == nullptr) {
    LOGE("PAGImage.LoadFromPath() Invalid image file : %s", path.c_str());
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGImage(pagImage));
}

PAG_API jlong Java_org_libpag_PAGImage_LoadFromBytes(JNIEnv* env, jclass, jbyteArray bytes,
                                                     jint length) {
  if (bytes == nullptr) {
    LOGE("PAGImage.LoadFromBytes() Invalid image bytes specified.");
    return 0;
  }
  auto data = env->GetByteArrayElements(bytes, nullptr);
  auto pagImage = PAGImage::FromBytes(data, static_cast<size_t>(length));
  env->ReleaseByteArrayElements(bytes, data, 0);
  if (pagImage == nullptr) {
    LOGE("PAGImage.LoadFromBytes() Invalid image bytes specified.");
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGImage(pagImage));
}

PAG_API jlong Java_org_libpag_PAGImage_LoadFromAssets(JNIEnv* env, jclass, jobject managerObj,
                                                      jstring pathObj) {
  auto path = SafeConvertToStdString(env, pathObj);
  auto byteData = ReadBytesFromAssets(env, managerObj, pathObj);
  if (byteData == nullptr) {
    LOGE("PAGImage.loadFromAssets() Can't find the file name from asset manager : %s",
         path.c_str());
    return 0;
  }
  auto pagImage = PAGImage::FromBytes(byteData->data(), byteData->length());
  if (pagImage == nullptr) {
    LOGE("PAGImage.LoadFromAssets() Invalid image file : %s", path.c_str());
    return 0;
  }
  return reinterpret_cast<jlong>(new JPAGImage(pagImage));
}

PAG_API jlong Java_org_libpag_PAGImage_LoadFromTexture(JNIEnv*, jclass, jint textureID,
                                                       jint textureTarget, jint width, jint height,
                                                       jboolean flipY) {
  GLTextureInfo textureInfo = {};
  textureInfo.target = static_cast<unsigned>(textureTarget);
  textureInfo.id = static_cast<unsigned>(textureID);
  BackendTexture backendTexture = {textureInfo, width, height};
  auto origin = flipY ? ImageOrigin::BottomLeft : ImageOrigin::TopLeft;
  auto image = PAGImage::FromTexture(backendTexture, origin);
  if (image == nullptr) {
    return 0;
  }
  auto pagImage = new JPAGImage(image);
  return reinterpret_cast<jlong>(pagImage);
}

PAG_API jint Java_org_libpag_PAGImage_width(JNIEnv* env, jobject thiz) {
  auto image = getPAGImage(env, thiz);
  if (image == nullptr) {
    return 0;
  }
  return image->width();
}

PAG_API jint Java_org_libpag_PAGImage_height(JNIEnv* env, jobject thiz) {
  auto image = getPAGImage(env, thiz);
  if (image == nullptr) {
    return 0;
  }
  return image->height();
}

PAG_API jint Java_org_libpag_PAGImage_scaleMode(JNIEnv* env, jobject thiz) {
  auto image = getPAGImage(env, thiz);
  if (image == nullptr) {
    return 0;
  }
  return static_cast<jint>(image->scaleMode());
}

PAG_API void Java_org_libpag_PAGImage_setScaleMode(JNIEnv* env, jobject thiz, jint value) {
  auto image = getPAGImage(env, thiz);
  if (image == nullptr) {
    return;
  }
  image->setScaleMode(static_cast<PAGScaleMode>(value));
}

PAG_API void Java_org_libpag_PAGImage_nativeGetMatrix(JNIEnv* env, jobject thiz,
                                                      jfloatArray values) {
  auto list = env->GetFloatArrayElements(values, nullptr);
  auto image = getPAGImage(env, thiz);
  if (image != nullptr) {
    auto matrix = image->matrix();
    matrix.get9(list);
  } else {
    tgfx::Matrix matrix = {};
    matrix.setIdentity();
    matrix.get9(list);
  }
  env->SetFloatArrayRegion(values, 0, 9, list);
  env->ReleaseFloatArrayElements(values, list, 0);
}

PAG_API void Java_org_libpag_PAGImage_nativeSetMatrix(JNIEnv* env, jobject thiz, jfloat a, jfloat b,
                                                      jfloat c, jfloat d, jfloat tx, jfloat ty) {
  auto image = getPAGImage(env, thiz);
  if (image == nullptr) {
    return;
  }
  Matrix matrix = {};
  matrix.setAll(a, c, tx, b, d, ty, 0, 0, 1);
  image->setMatrix(matrix);
}
}
