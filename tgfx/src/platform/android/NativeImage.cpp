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

#include "NativeImage.h"
#include <android/bitmap.h>
#include "platform/NativeCodec.h"
#include "tgfx/core/Bitmap.h"

namespace tgfx {
static constexpr int BITMAP_FLAGS_ALPHA_UNPREMUL = 2;

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

void NativeImage::JNIInit(JNIEnv* env) {
  BitmapFactoryOptionsClass.reset(env, env->FindClass("android/graphics/BitmapFactory$Options"));
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
  ByteArrayInputStreamClass.reset(env, env->FindClass("java/io/ByteArrayInputStream"));
  ByteArrayInputStream_Constructor =
      env->GetMethodID(ByteArrayInputStreamClass.get(), "<init>", "([B)V");
  ExifInterfaceClass.reset(env, env->FindClass("androidx/exifinterface/media/ExifInterface"));
  ExifInterface_Constructor_Path =
      env->GetMethodID(ExifInterfaceClass.get(), "<init>", "(Ljava/lang/String;)V");
  ExifInterface_Constructor_Stream =
      env->GetMethodID(ExifInterfaceClass.get(), "<init>", "(Ljava/io/InputStream;)V");
  ExifInterfaceClass_getAttributeInt =
      env->GetMethodID(ExifInterfaceClass.get(), "getAttributeInt", "(Ljava/lang/String;I)I");
  BitmapFactoryClass.reset(env, env->FindClass("android/graphics/BitmapFactory"));
  BitmapFactory_decodeFile = env->GetStaticMethodID(
      BitmapFactoryClass.get(), "decodeFile",
      "(Ljava/lang/String;Landroid/graphics/BitmapFactory$Options;)Landroid/graphics/Bitmap;");
  BitmapFactory_decodeByteArray = env->GetStaticMethodID(
      BitmapFactoryClass.get(), "decodeByteArray",
      "([BIILandroid/graphics/BitmapFactory$Options;)Landroid/graphics/Bitmap;");
  BitmapConfigClass.reset(env, env->FindClass("android/graphics/Bitmap$Config"));
  BitmapConfig_ALPHA_8 =
      env->GetStaticFieldID(BitmapConfigClass.get(), "ALPHA_8", "Landroid/graphics/Bitmap$Config;");
  BitmapConfig_ARGB_8888 = env->GetStaticFieldID(BitmapConfigClass.get(), "ARGB_8888",
                                                 "Landroid/graphics/Bitmap$Config;");
}

std::shared_ptr<NativeImage> NativeImage::Make(JNIEnv* env, jobject sizeObject, int orientation) {
  auto size = env->GetIntArrayElements(static_cast<jintArray>(sizeObject), nullptr);
  int width = size[0];
  int height = size[1];
  env->ReleaseIntArrayElements(static_cast<jintArray>(sizeObject), size, 0);
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  return std::unique_ptr<NativeImage>(
      new NativeImage(width, height, static_cast<Orientation>(orientation)));
}

static Orientation GetOrientation(JNIEnv* env, jobject exifInterface) {
  if (exifInterface == nullptr) {
    env->ExceptionClear();
    return Orientation::TopLeft;
  }
  Local<jstring> key = {env, env->NewStringUTF("Orientation")};
  auto orientation = env->CallIntMethod(exifInterface, ExifInterfaceClass_getAttributeInt,
                                        key.get(), static_cast<int>(Orientation::TopLeft));
  return static_cast<Orientation>(orientation);
}

static jobject DecodeBitmap(JNIEnv* env, jobject options, const std::string& filePath) {
  Local<jstring> imagePath = {env, SafeToJString(env, filePath)};
  return env->CallStaticObjectMethod(BitmapFactoryClass.get(), BitmapFactory_decodeFile,
                                     imagePath.get(), options);
}

std::shared_ptr<Image> NativeCodec::MakeImage(const std::string& filePath) {
  auto env = CurrentJNIEnv();
  if (env == nullptr || filePath.empty()) {
    return nullptr;
  }
  Local<jobject> options = {
      env, env->NewObject(BitmapFactoryOptionsClass.get(), BitmapFactoryOptions_Constructor)};
  if (options.empty()) {
    return nullptr;
  }
  env->SetBooleanField(options.get(), BitmapFactoryOptions_inJustDecodeBounds, true);
  DecodeBitmap(env, options.get(), filePath);
  auto width = env->GetIntField(options.get(), BitmapFactoryOptions_outWidth);
  auto height = env->GetIntField(options.get(), BitmapFactoryOptions_outHeight);
  if (width <= 0 || height <= 0) {
    env->ExceptionClear();
    return nullptr;
  }
  Local<jstring> imagePath = {env, SafeToJString(env, filePath)};
  Local<jobject> exifInterface = {
      env,
      env->NewObject(ExifInterfaceClass.get(), ExifInterface_Constructor_Path, imagePath.get())};
  auto orientation = GetOrientation(env, exifInterface.get());
  auto codec = std::unique_ptr<NativeImage>(new NativeImage(width, height, orientation));
  codec->imagePath = filePath;
  return codec;
}

static jobject DecodeBitmap(JNIEnv* env, jobject options, std::shared_ptr<Data> imageBytes) {
  Local<jbyteArray> byteArray = {env, env->NewByteArray(imageBytes->size())};
  env->SetByteArrayRegion(byteArray.get(), 0, imageBytes->size(),
                          reinterpret_cast<const jbyte*>(imageBytes->data()));
  return env->CallStaticObjectMethod(BitmapFactoryClass.get(), BitmapFactory_decodeByteArray,
                                     byteArray.get(), 0, imageBytes->size(), options);
}

std::shared_ptr<Image> NativeCodec::MakeImage(std::shared_ptr<Data> imageBytes) {
  auto env = CurrentJNIEnv();
  if (env == nullptr || imageBytes == nullptr) {
    return nullptr;
  }
  Local<jobject> options = {
      env, env->NewObject(BitmapFactoryOptionsClass.get(), BitmapFactoryOptions_Constructor)};
  if (options.empty()) {
    return nullptr;
  }
  env->SetBooleanField(options.get(), BitmapFactoryOptions_inJustDecodeBounds, true);
  DecodeBitmap(env, options.get(), imageBytes);
  auto width = env->GetIntField(options.get(), BitmapFactoryOptions_outWidth);
  auto height = env->GetIntField(options.get(), BitmapFactoryOptions_outHeight);
  if (width <= 0 || height <= 0) {
    env->ExceptionClear();
    return nullptr;
  }
  Local<jbyteArray> byteArray = {env, env->NewByteArray(imageBytes->size())};
  env->SetByteArrayRegion(byteArray.get(), 0, imageBytes->size(),
                          reinterpret_cast<const jbyte*>(imageBytes->data()));
  Local<jobject> inputStream = {
      env, env->NewObject(ByteArrayInputStreamClass.get(), ByteArrayInputStream_Constructor,
                          byteArray.get())};
  Local<jobject> exifInterface = {
      env, env->NewObject(ExifInterfaceClass.get(), ExifInterface_Constructor_Stream,
                          inputStream.get())};
  auto orientation = GetOrientation(env, exifInterface.get());
  auto codec = std::unique_ptr<NativeImage>(new NativeImage(width, height, orientation));
  codec->imageBytes = imageBytes;
  return codec;
}

static ImageInfo GetImageInfo(JNIEnv* env, jobject bitmap) {
  AndroidBitmapInfo bitmapInfo = {};
  if (bitmap == nullptr || AndroidBitmap_getInfo(env, bitmap, &bitmapInfo) != 0) {
    return {};
  }
  AlphaType alphaType = (bitmapInfo.flags & BITMAP_FLAGS_ALPHA_UNPREMUL)
                            ? AlphaType::Unpremultiplied
                            : AlphaType::Premultiplied;
  ColorType colorType;
  switch (bitmapInfo.format) {
    case ANDROID_BITMAP_FORMAT_RGBA_8888:
      colorType = ColorType::RGBA_8888;
      break;
    case ANDROID_BITMAP_FORMAT_A_8:
      colorType = ColorType::ALPHA_8;
      break;
    default:
      colorType = ColorType::Unknown;
      break;
  }
  return ImageInfo::Make(bitmapInfo.width, bitmapInfo.height, colorType, alphaType,
                         bitmapInfo.stride);
}

std::shared_ptr<Image> NativeCodec::MakeFrom(void* nativeImage) {
  auto env = CurrentJNIEnv();
  if (env == nullptr) {
    return nullptr;
  }
  auto bitmap = reinterpret_cast<jobject>(nativeImage);
  auto info = GetImageInfo(env, bitmap);
  if (info.isEmpty()) {
    env->ExceptionClear();
    return nullptr;
  }
  auto image = std::unique_ptr<NativeImage>(
      new NativeImage(info.width(), info.height(), Orientation::TopLeft));
  image->bitmap.reset(env, bitmap);
  return image;
}

bool NativeImage::readPixels(const ImageInfo& dstInfo, void* dstPixels) const {
  if (dstInfo.isEmpty() || dstPixels == nullptr) {
    return false;
  }
  auto env = CurrentJNIEnv();
  if (env == nullptr) {
    return false;
  }
  Local<jobject> bitmapObject;
  if (!bitmap.empty()) {
    bitmapObject.reset(env, env->NewLocalRef(bitmap.get()));
  } else {
    Local<jobject> options = {
        env, env->NewObject(BitmapFactoryOptionsClass.get(), BitmapFactoryOptions_Constructor)};
    if (options.empty()) {
      env->ExceptionClear();
      return false;
    }
    Local<jobject> config;
    if (dstInfo.colorType() == ColorType::ALPHA_8) {
      config.reset(env, env->GetStaticObjectField(BitmapConfigClass.get(), BitmapConfig_ALPHA_8));
    } else {
      config.reset(env, env->GetStaticObjectField(BitmapConfigClass.get(), BitmapConfig_ARGB_8888));
    }
    env->SetObjectField(options.get(), BitmapFactoryOptions_inPreferredConfig, config.get());
    if (dstInfo.alphaType() == AlphaType::Unpremultiplied) {
      env->SetBooleanField(options.get(), BitmapFactoryOptions_inPremultiplied, false);
    }
    if (!imagePath.empty()) {
      bitmapObject.reset(env, DecodeBitmap(env, options.get(), imagePath));
    } else {
      bitmapObject.reset(env, DecodeBitmap(env, options.get(), imageBytes));
    }
  }
  auto info = GetImageInfo(env, bitmapObject.get());
  if (info.isEmpty()) {
    env->ExceptionClear();
    return false;
  }
  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmapObject.get(), &pixels) != 0) {
    env->ExceptionClear();
    return false;
  }
  auto result = Bitmap(info, pixels).readPixels(dstInfo, dstPixels);
  AndroidBitmap_unlockPixels(env, bitmapObject.get());
  return result;
}
}  // namespace tgfx
