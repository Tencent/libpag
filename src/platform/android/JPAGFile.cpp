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
#include "JPAGImage.h"
#include "JPAGLayerHandle.h"
#include "NativePlatform.h"
#include "PAGText.h"

namespace pag {
static jfieldID PAGFile_nativeContext;
}

using namespace pag;

std::shared_ptr<pag::PAGFile> getPAGFile(JNIEnv* env, jobject thiz) {
  auto nativeContext =
      reinterpret_cast<JPAGLayerHandle*>(env->GetLongField(thiz, PAGFile_nativeContext));
  if (nativeContext == nullptr) {
    return nullptr;
  }
  return std::static_pointer_cast<PAGFile>(nativeContext->get());
}

extern "C" {

PAG_API void Java_org_libpag_PAGFile_nativeInit(JNIEnv* env, jclass clazz) {
  PAGFile_nativeContext = env->GetFieldID(clazz, "nativeContext", "J");
}

PAG_API jint Java_org_libpag_PAGFile_MaxSupportedTagLevel(JNIEnv*, jclass) {
  return pag::PAGFile::MaxSupportedTagLevel();
}

PAG_API jobject Java_org_libpag_PAGFile_LoadFromPath(JNIEnv* env, jclass, jstring pathObj) {
  if (pathObj == nullptr) {
    LOGE("PAGFile.LoadFromPath() Invalid path specified.");
    return NULL;
  }
  auto path = SafeConvertToStdString(env, pathObj);
  if (path.empty()) {
    return NULL;
  }
  LOGI("PAGFile.LoadFromPath() start: %s", path.c_str());
  auto pagFile = PAGFile::Load(path);
  if (pagFile == nullptr) {
    LOGE("PAGFile.LoadFromPath() Invalid pag file : %s", path.c_str());
    return NULL;
  }
  return ToPAGLayerJavaObject(env, pagFile);
}

PAG_API jobject Java_org_libpag_PAGFile_LoadFromBytes(JNIEnv* env, jclass, jbyteArray bytes,
                                                      jint length, jstring jpath) {
  if (bytes == nullptr) {
    LOGE("PAGFile.LoadFromBytes() Invalid pag file bytes specified.");
    return NULL;
  }
  auto data = env->GetByteArrayElements(bytes, nullptr);
  auto path = SafeConvertToStdString(env, jpath);
  auto pagFile = PAGFile::Load(data, static_cast<size_t>(length), path);
  env->ReleaseByteArrayElements(bytes, data, 0);
  if (pagFile == nullptr) {
    LOGE("PAGFile.LoadFromBytes() Invalid pag file bytes specified.");
    return NULL;
  }
  return ToPAGLayerJavaObject(env, pagFile);
}

PAG_API jobject Java_org_libpag_PAGFile_LoadFromAssets(JNIEnv* env, jclass, jobject managerObj,
                                                       jstring pathObj) {
  auto path = SafeConvertToStdString(env, pathObj);
  auto byteData = ReadBytesFromAssets(env, managerObj, pathObj);
  if (byteData == nullptr) {
    LOGE("PAGFile.LoadFromAssets() Can't find the file name from asset manager : %s", path.c_str());
    return NULL;
  }
  LOGI("PAGFile.LoadFromAssets() start: %s", path.c_str());
  auto pagFile = PAGFile::Load(byteData->data(), byteData->length(), "assets://" + path);
  if (pagFile == nullptr) {
    LOGE("PAGFile.LoadFromAssets() Invalid pag file : %s", path.c_str());
    return NULL;
  }
  return ToPAGLayerJavaObject(env, pagFile);
}

PAG_API jint Java_org_libpag_PAGFile_tagLevel(JNIEnv* env, jobject thiz) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->tagLevel();
}

PAG_API jint Java_org_libpag_PAGFile_numTexts(JNIEnv* env, jobject thiz) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->numTexts();
}

PAG_API jint Java_org_libpag_PAGFile_numImages(JNIEnv* env, jobject thiz) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->numImages();
}

PAG_API jint Java_org_libpag_PAGFile_numVideos(JNIEnv* env, jobject thiz) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return 0;
  }
  return pagFile->numVideos();
}

PAG_API jstring Java_org_libpag_PAGFile_path(JNIEnv* env, jobject thiz) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return 0;
  }
  auto path = pagFile->path();
  return SafeConvertToJString(env, path);
}

PAG_API jobject Java_org_libpag_PAGFile_getTextData(JNIEnv* env, jobject thiz, jint index) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return nullptr;
  }
  auto textDocument = pagFile->getTextData(index);
  return ToPAGTextObject(env, textDocument);
}

PAG_API void Java_org_libpag_PAGFile_replaceText(JNIEnv* env, jobject thiz, jint index,
                                                 jobject textData) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return;
  }
  auto textDocument = ToTextDocument(env, textData);
  pagFile->replaceText(index, textDocument);
}

PAG_API void Java_org_libpag_PAGFile_nativeReplaceImage(JNIEnv* env, jobject thiz, jint index,
                                                        jlong imageObject) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return;
  }
  auto image = reinterpret_cast<JPAGImage*>(imageObject);
  if (image != nullptr) {
    pagFile->replaceImage(index, image->get());
  } else {
    pagFile->replaceImage(index, nullptr);
  }
}

PAG_API void Java_org_libpag_PAGFile_nativeReplaceImageByName(JNIEnv* env, jobject thiz,
                                                              jstring layerName, long imageObject) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return;
  }
  auto name = SafeConvertToStdString(env, layerName);
  if (name.empty()) {
    return;
  }
  auto image = reinterpret_cast<JPAGImage*>(imageObject);
  if (image != nullptr) {
    pagFile->replaceImageByName(name, image->get());
  } else {
    pagFile->replaceImageByName(name, nullptr);
  }
}

PAG_API jobjectArray Java_org_libpag_PAGFile_getLayersByEditableIndex(JNIEnv* env, jobject thiz,
                                                                      jint editableIndex,
                                                                      jint layerType) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return ToPAGLayerJavaObjectList(env, {});
  }
  auto layers =
      pagFile->getLayersByEditableIndex(editableIndex, static_cast<pag::LayerType>(layerType));
  return ToPAGLayerJavaObjectList(env, layers);
}

PAG_API jintArray Java_org_libpag_PAGFile_getEditableIndices(JNIEnv* env, jobject thiz,
                                                             jint layerType) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return env->NewIntArray(0);
  }
  auto indices = pagFile->getEditableIndices(static_cast<pag::LayerType>(layerType));
  auto result = env->NewIntArray(indices.size());
  env->SetIntArrayRegion(result, 0, indices.size(), indices.data());
  return result;
}

PAG_API jint Java_org_libpag_PAGFile_timeStretchMode(JNIEnv* env, jobject thiz) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return 0;
  }
  return static_cast<jint>(pagFile->timeStretchMode());
}

PAG_API void Java_org_libpag_PAGFile_setTimeStretchMode(JNIEnv* env, jobject thiz, jint mode) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return;
  }
  pagFile->setTimeStretchMode(static_cast<PAGTimeStretchMode>(mode));
}

PAG_API void Java_org_libpag_PAGFile_setDuration(JNIEnv* env, jobject thiz, jlong duration) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return;
  }
  pagFile->setDuration(duration);
}

PAG_API jobject Java_org_libpag_PAGFile_copyOriginal(JNIEnv* env, jobject thiz) {
  auto pagFile = getPAGFile(env, thiz);
  if (pagFile == nullptr) {
    return NULL;
  }
  auto newFile = pagFile->copyOriginal();
  return ToPAGLayerJavaObject(env, newFile);
}
}
