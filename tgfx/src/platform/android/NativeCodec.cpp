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

#include "NativeCodec.h"
#include <android/bitmap.h>
#include "NativeImageInfo.h"
#include "core/utils/Log.h"
#include "tgfx/core/Bitmap.h"

namespace tgfx {
static Global<jclass> BitmapFactoryOptionsClass;
static jmethodID BitmapFactoryOptions_Constructor;
static jfieldID BitmapFactoryOptions_inJustDecodeBounds;
static jfieldID BitmapFactoryOptions_inPreferredConfig;
static jfieldID BitmapFactoryOptions_inPremultiplied;
static jfieldID BitmapFactoryOptions_outWidth;
static jfieldID BitmapFactoryOptions_outHeight;
static Global<jclass> BitmapFactoryClass;
static jmethodID BitmapFactory_decodeFile;
static jmethodID BitmapFactory_decodeByteArray;
static Global<jclass> ByteArrayInputStreamClass;
static jmethodID ByteArrayInputStream_Constructor;
static Global<jclass> ExifInterfaceClass;
static jmethodID ExifInterface_Constructor_Path;
static jmethodID ExifInterface_Constructor_Stream;
static jmethodID ExifInterfaceClass_getAttributeInt;
static Global<jclass> BitmapConfigClass;
static jfieldID BitmapConfig_ALPHA_8;
static jfieldID BitmapConfig_ARGB_8888;

void NativeCodec::JNIInit(JNIEnv* env) {
  BitmapFactoryOptionsClass = env->FindClass("android/graphics/BitmapFactory$Options");
  BitmapFactoryOptions_Constructor =
      env->GetMethodID(BitmapFactoryOptionsClass.get(), "<init>", "()V");
  BitmapFactoryOptions_inJustDecodeBounds =
      env->GetFieldID(BitmapFactoryOptionsClass.get(), "inJustDecodeBounds", "Z");
  BitmapFactoryOptions_inPreferredConfig = env->GetFieldID(
      BitmapFactoryOptionsClass.get(), "inPreferredConfig", "Landroid/graphics/Bitmap$Config;");
  BitmapFactoryOptions_inPremultiplied =
      env->GetFieldID(BitmapFactoryOptionsClass.get(), "inPremultiplied", "Z");
  BitmapFactoryOptions_outWidth = env->GetFieldID(BitmapFactoryOptionsClass.get(), "outWidth", "I");
  BitmapFactoryOptions_outHeight =
      env->GetFieldID(BitmapFactoryOptionsClass.get(), "outHeight", "I");
  ByteArrayInputStreamClass = env->FindClass("java/io/ByteArrayInputStream");
  ByteArrayInputStream_Constructor =
      env->GetMethodID(ByteArrayInputStreamClass.get(), "<init>", "([B)V");
  ExifInterfaceClass = env->FindClass("androidx/exifinterface/media/ExifInterface");
  ExifInterface_Constructor_Path =
      env->GetMethodID(ExifInterfaceClass.get(), "<init>", "(Ljava/lang/String;)V");
  ExifInterface_Constructor_Stream =
      env->GetMethodID(ExifInterfaceClass.get(), "<init>", "(Ljava/io/InputStream;)V");
  ExifInterfaceClass_getAttributeInt =
      env->GetMethodID(ExifInterfaceClass.get(), "getAttributeInt", "(Ljava/lang/String;I)I");
  BitmapFactoryClass = env->FindClass("android/graphics/BitmapFactory");
  BitmapFactory_decodeFile = env->GetStaticMethodID(
      BitmapFactoryClass.get(), "decodeFile",
      "(Ljava/lang/String;Landroid/graphics/BitmapFactory$Options;)Landroid/graphics/Bitmap;");
  BitmapFactory_decodeByteArray = env->GetStaticMethodID(
      BitmapFactoryClass.get(), "decodeByteArray",
      "([BIILandroid/graphics/BitmapFactory$Options;)Landroid/graphics/Bitmap;");
  BitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
  BitmapConfig_ALPHA_8 =
      env->GetStaticFieldID(BitmapConfigClass.get(), "ALPHA_8", "Landroid/graphics/Bitmap$Config;");
  BitmapConfig_ARGB_8888 = env->GetStaticFieldID(BitmapConfigClass.get(), "ARGB_8888",
                                                 "Landroid/graphics/Bitmap$Config;");
}

std::shared_ptr<NativeCodec> NativeCodec::Make(JNIEnv* env, jobject sizeObject, int orientation) {
  auto size = env->GetIntArrayElements(static_cast<jintArray>(sizeObject), nullptr);
  int width = size[0];
  int height = size[1];
  env->ReleaseIntArrayElements(static_cast<jintArray>(sizeObject), size, 0);
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  return std::shared_ptr<NativeCodec>(
      new NativeCodec(width, height, static_cast<Orientation>(orientation)));
}

static Orientation GetOrientation(JNIEnv* env, jobject exifInterface) {
  if (exifInterface == nullptr) {
    env->ExceptionClear();
    return Orientation::TopLeft;
  }
  auto key = env->NewStringUTF("Orientation");
  auto orientation = env->CallIntMethod(exifInterface, ExifInterfaceClass_getAttributeInt, key,
                                        static_cast<int>(Orientation::TopLeft));
  return static_cast<Orientation>(orientation);
}

static jobject DecodeBitmap(JNIEnv* env, jobject options, const std::string& filePath) {
  auto imagePath = SafeToJString(env, filePath);
  auto bitmap = env->CallStaticObjectMethod(BitmapFactoryClass.get(), BitmapFactory_decodeFile,
                                            imagePath, options);
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  return bitmap;
}

std::shared_ptr<ImageCodec> ImageCodec::MakeNativeCodec(const std::string& filePath) {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr || filePath.empty()) {
    return nullptr;
  }
  auto options = env->NewObject(BitmapFactoryOptionsClass.get(), BitmapFactoryOptions_Constructor);
  if (options == nullptr) {
    return nullptr;
  }
  env->SetBooleanField(options, BitmapFactoryOptions_inJustDecodeBounds, true);
  DecodeBitmap(env, options, filePath);
  auto width = env->GetIntField(options, BitmapFactoryOptions_outWidth);
  auto height = env->GetIntField(options, BitmapFactoryOptions_outHeight);
  if (width <= 0 || height <= 0) {
    env->ExceptionClear();
    LOGE("NativeCodec::readPixels(): Failed to get the size of the image!");
    return nullptr;
  }
  auto imagePath = SafeToJString(env, filePath);
  auto exifInterface =
      env->NewObject(ExifInterfaceClass.get(), ExifInterface_Constructor_Path, imagePath);
  auto orientation = GetOrientation(env, exifInterface);
  auto codec = std::shared_ptr<NativeCodec>(new NativeCodec(width, height, orientation));
  codec->imagePath = filePath;
  return codec;
}

static jobject DecodeBitmap(JNIEnv* env, jobject options, std::shared_ptr<Data> imageBytes) {
  auto byteArray = env->NewByteArray(imageBytes->size());
  env->SetByteArrayRegion(byteArray, 0, imageBytes->size(),
                          reinterpret_cast<const jbyte*>(imageBytes->data()));
  auto bitmap = env->CallStaticObjectMethod(BitmapFactoryClass.get(), BitmapFactory_decodeByteArray,
                                            byteArray, 0, imageBytes->size(), options);
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  return bitmap;
}

std::shared_ptr<ImageCodec> ImageCodec::MakeNativeCodec(std::shared_ptr<Data> imageBytes) {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr || imageBytes == nullptr) {
    return nullptr;
  }
  auto options = env->NewObject(BitmapFactoryOptionsClass.get(), BitmapFactoryOptions_Constructor);
  if (options == nullptr) {
    return nullptr;
  }
  env->SetBooleanField(options, BitmapFactoryOptions_inJustDecodeBounds, true);
  DecodeBitmap(env, options, imageBytes);
  auto width = env->GetIntField(options, BitmapFactoryOptions_outWidth);
  auto height = env->GetIntField(options, BitmapFactoryOptions_outHeight);
  if (width <= 0 || height <= 0) {
    env->ExceptionClear();
    LOGE("NativeCodec::readPixels(): Failed to get the size of the image!");
    return nullptr;
  }
  auto byteArray = env->NewByteArray(imageBytes->size());
  env->SetByteArrayRegion(byteArray, 0, imageBytes->size(),
                          reinterpret_cast<const jbyte*>(imageBytes->data()));
  auto inputStream =
      env->NewObject(ByteArrayInputStreamClass.get(), ByteArrayInputStream_Constructor, byteArray);
  auto exifInterface =
      env->NewObject(ExifInterfaceClass.get(), ExifInterface_Constructor_Stream, inputStream);
  auto orientation = GetOrientation(env, exifInterface);
  auto codec = std::shared_ptr<NativeCodec>(new NativeCodec(width, height, orientation));
  codec->imageBytes = imageBytes;
  return codec;
}

bool NativeCodec::readPixels(const ImageInfo& dstInfo, void* dstPixels) const {
  if (dstInfo.isEmpty() || dstPixels == nullptr) {
    return false;
  }
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return false;
  }
  auto options = env->NewObject(BitmapFactoryOptionsClass.get(), BitmapFactoryOptions_Constructor);
  if (options == nullptr) {
    env->ExceptionClear();
    LOGE("NativeCodec::readPixels(): Failed to create a BitmapFactory.Options object!");
    return false;
  }
  jobject config;
  if (dstInfo.colorType() == ColorType::ALPHA_8) {
    config = env->GetStaticObjectField(BitmapConfigClass.get(), BitmapConfig_ALPHA_8);
  } else {
    config = env->GetStaticObjectField(BitmapConfigClass.get(), BitmapConfig_ARGB_8888);
  }
  env->SetObjectField(options, BitmapFactoryOptions_inPreferredConfig, config);
  if (dstInfo.alphaType() == AlphaType::Unpremultiplied) {
    env->SetBooleanField(options, BitmapFactoryOptions_inPremultiplied, false);
  }
  jobject bitmap;
  if (!imagePath.empty()) {
    bitmap = DecodeBitmap(env, options, imagePath);
  } else {
    bitmap = DecodeBitmap(env, options, imageBytes);
  }
  auto info = NativeImageInfo::GetInfo(env, bitmap);
  if (info.isEmpty()) {
    env->ExceptionClear();
    LOGE("NativeCodec::readPixels(): Failed to decode the image!");
    return false;
  }
  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != 0) {
    env->ExceptionClear();
    LOGE("NativeCodec::readPixels(): Failed to lockPixels() of a Java bitmap!");
    return false;
  }
  auto result = Bitmap(info, pixels).readPixels(dstInfo, dstPixels);
  AndroidBitmap_unlockPixels(env, bitmap);
  return result;
}
}  // namespace tgfx
