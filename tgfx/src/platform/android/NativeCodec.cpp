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
#include "HardwareBuffer.h"
#include "NativeImageBuffer.h"
#include "tgfx/core/Pixmap.h"
#include "tgfx/platform/android/AndroidBitmap.h"
#include "utils/Log.h"

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
static Global<jclass> BitmapClass;
static jmethodID Bitmap_copy;
static jmethodID Bitmap_getConfig;
static Global<jclass> BitmapConfigClass;
static jmethodID BitmapConfig_equals;
static jfieldID BitmapConfig_ALPHA_8;
static jfieldID BitmapConfig_ARGB_8888;
static jfieldID BitmapConfig_RGB_565;
static jfieldID BitmapConfig_HARDWARE;

void NativeCodec::JNIInit(JNIEnv* env) {
  BitmapFactoryOptionsClass = env->FindClass("android/graphics/BitmapFactory$Options");
  if (BitmapFactoryOptionsClass.get() == nullptr) {
    LOGE("Could not run NativeCodec.InitJNI(), BitmapFactoryOptionsClass is not found!");
    return;
  }
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
  BitmapClass = env->FindClass("android/graphics/Bitmap");
  Bitmap_copy = env->GetMethodID(BitmapClass.get(), "copy",
                                 "(Landroid/graphics/Bitmap$Config;Z)Landroid/graphics/Bitmap;");
  Bitmap_getConfig =
      env->GetMethodID(BitmapClass.get(), "getConfig", "()Landroid/graphics/Bitmap$Config;");
  BitmapConfigClass = env->FindClass("android/graphics/Bitmap$Config");
  BitmapConfig_equals =
      env->GetMethodID(BitmapConfigClass.get(), "equals", "(Ljava/lang/Object;)Z");
  BitmapConfig_ALPHA_8 =
      env->GetStaticFieldID(BitmapConfigClass.get(), "ALPHA_8", "Landroid/graphics/Bitmap$Config;");
  BitmapConfig_ARGB_8888 = env->GetStaticFieldID(BitmapConfigClass.get(), "ARGB_8888",
                                                 "Landroid/graphics/Bitmap$Config;");
  BitmapConfig_RGB_565 =
      env->GetStaticFieldID(BitmapConfigClass.get(), "RGB_565", "Landroid/graphics/Bitmap$Config;");
  BitmapConfig_HARDWARE = env->GetStaticFieldID(BitmapConfigClass.get(), "HARDWARE",
                                                "Landroid/graphics/Bitmap$Config;");
  // BitmapConfig_HARDWARE may be nullptr.
  if (env->ExceptionCheck()) {
    env->ExceptionClear();
  }
}

std::shared_ptr<NativeCodec> NativeCodec::Make(JNIEnv* env, jobject sizeObject, int origin) {
  auto size = env->GetIntArrayElements(static_cast<jintArray>(sizeObject), nullptr);
  int width = size[0];
  int height = size[1];
  env->ReleaseIntArrayElements(static_cast<jintArray>(sizeObject), size, 0);
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  return std::shared_ptr<NativeCodec>(
      new NativeCodec(width, height, static_cast<EncodedOrigin>(origin)));
}

static EncodedOrigin GetEncodedOrigin(JNIEnv* env, jobject exifInterface) {
  if (exifInterface == nullptr) {
    env->ExceptionClear();
    return EncodedOrigin::TopLeft;
  }
  auto key = env->NewStringUTF("Orientation");
  auto origin = env->CallIntMethod(exifInterface, ExifInterfaceClass_getAttributeInt, key,
                                   static_cast<int>(EncodedOrigin::TopLeft));
  return static_cast<EncodedOrigin>(origin);
}

std::shared_ptr<ImageCodec> ImageCodec::MakeNativeCodec(const std::string& filePath) {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr || filePath.empty()) {
    return nullptr;
  }
  if (BitmapFactoryOptionsClass.get() == nullptr) {
    LOGE("Could not run NativeCodec.GetEncodedOrigin(), BitmapFactoryOptionsClass is not found!");
    return nullptr;
  }
  auto options = env->NewObject(BitmapFactoryOptionsClass.get(), BitmapFactoryOptions_Constructor);
  if (options == nullptr) {
    return nullptr;
  }
  env->SetBooleanField(options, BitmapFactoryOptions_inJustDecodeBounds, true);
  auto imagePath = SafeToJString(env, filePath);
  env->CallStaticObjectMethod(BitmapFactoryClass.get(), BitmapFactory_decodeFile, imagePath,
                              options);
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  auto width = env->GetIntField(options, BitmapFactoryOptions_outWidth);
  auto height = env->GetIntField(options, BitmapFactoryOptions_outHeight);
  if (width <= 0 || height <= 0) {
    env->ExceptionClear();
    LOGE("NativeCodec::readPixels(): Failed to get the size of the image!");
    return nullptr;
  }
  auto exifInterface =
      env->NewObject(ExifInterfaceClass.get(), ExifInterface_Constructor_Path, imagePath);
  auto origin = GetEncodedOrigin(env, exifInterface);
  auto codec = std::shared_ptr<NativeCodec>(new NativeCodec(width, height, origin));
  codec->imagePath = filePath;
  return codec;
}

std::shared_ptr<ImageCodec> ImageCodec::MakeNativeCodec(std::shared_ptr<Data> imageBytes) {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr || imageBytes == nullptr) {
    return nullptr;
  }
  if (BitmapFactoryOptionsClass.get() == nullptr) {
    LOGE("Could not run NativeCodec.MakeNativeCodec(), BitmapFactoryOptionsClass is not found!");
    return nullptr;
  }
  auto options = env->NewObject(BitmapFactoryOptionsClass.get(), BitmapFactoryOptions_Constructor);
  if (options == nullptr) {
    return nullptr;
  }
  env->SetBooleanField(options, BitmapFactoryOptions_inJustDecodeBounds, true);
  auto byteArray = env->NewByteArray(imageBytes->size());
  env->SetByteArrayRegion(byteArray, 0, imageBytes->size(),
                          reinterpret_cast<const jbyte*>(imageBytes->data()));
  env->CallStaticObjectMethod(BitmapFactoryClass.get(), BitmapFactory_decodeByteArray, byteArray, 0,
                              imageBytes->size(), options);
  if (env->ExceptionCheck()) {
    return nullptr;
  }
  auto width = env->GetIntField(options, BitmapFactoryOptions_outWidth);
  auto height = env->GetIntField(options, BitmapFactoryOptions_outHeight);
  if (width <= 0 || height <= 0) {
    env->ExceptionClear();
    LOGE("NativeCodec::readPixels(): Failed to get the size of the image!");
    return nullptr;
  }
  auto inputStream =
      env->NewObject(ByteArrayInputStreamClass.get(), ByteArrayInputStream_Constructor, byteArray);
  auto exifInterface =
      env->NewObject(ExifInterfaceClass.get(), ExifInterface_Constructor_Stream, inputStream);
  auto origin = GetEncodedOrigin(env, exifInterface);
  auto codec = std::shared_ptr<NativeCodec>(new NativeCodec(width, height, origin));
  codec->imageBytes = imageBytes;
  return codec;
}

std::shared_ptr<ImageCodec> ImageCodec::MakeFrom(NativeImageRef nativeImage) {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return nullptr;
  }
  if (BitmapFactoryOptionsClass.get() == nullptr) {
    LOGE("Could not run NativeCodec.MakeNativeCodec(), BitmapFactoryOptionsClass is not found!");
    return nullptr;
  }
  auto info = AndroidBitmap::GetInfo(env, nativeImage);
  if (info.isEmpty()) {
    return nullptr;
  }
  auto image = std::shared_ptr<NativeCodec>(
      new NativeCodec(info.width(), info.height(), EncodedOrigin::TopLeft));
  image->nativeImage = nativeImage;
  return image;
}

static jobject ConvertHardwareBitmap(JNIEnv* env, jobject bitmap) {
  // The AndroidBitmapInfo does not contain the ANDROID_BITMAP_FLAGS_IS_HARDWARE flag in the old
  // versions of Android NDK, even when the Java Bitmap has the hardware config. So we check it here
  // by the Java-side methods.
  if (bitmap == nullptr) {
    return nullptr;
  }
  if (BitmapConfig_HARDWARE == nullptr) {
    return bitmap;
  }
  auto config = env->CallObjectMethod(bitmap, Bitmap_getConfig);
  if (config == nullptr) {
    return bitmap;
  }
  static Global<jobject> HardwareConfig =
      env->GetStaticObjectField(BitmapConfigClass.get(), BitmapConfig_HARDWARE);
  if (HardwareConfig.isEmpty()) {
    return bitmap;
  }
  auto result = env->CallBooleanMethod(config, BitmapConfig_equals, HardwareConfig.get());
  if (result) {
    static Global<jobject> RGBA_Config =
        env->GetStaticObjectField(BitmapConfigClass.get(), BitmapConfig_ARGB_8888);
    auto newBitmap = env->CallObjectMethod(bitmap, Bitmap_copy, RGBA_Config.get(), JNI_FALSE);
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
      return bitmap;
    }
    bitmap = newBitmap;
  }
  return bitmap;
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
  auto bitmap = decodeBitmap(env, dstInfo.colorType(), dstInfo.alphaType(), false);
  auto info = AndroidBitmap::GetInfo(env, bitmap);
  if (info.isEmpty()) {
    LOGE("NativeCodec::readPixels() Failed to read the image info from a Bitmap!");
    return false;
  }
  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != 0) {
    env->ExceptionClear();
    LOGE("NativeCodec::readPixels() Failed to lockPixels() of a Java Bitmap!");
    return false;
  }
  auto result = tgfx::Pixmap(info, pixels).readPixels(dstInfo, dstPixels);
  AndroidBitmap_unlockPixels(env, bitmap);
  return result;
}

std::shared_ptr<ImageBuffer> NativeCodec::onMakeBuffer(bool tryHardware) const {
  JNIEnvironment environment;
  auto env = environment.current();
  if (env == nullptr) {
    return nullptr;
  }
  auto bitmap = decodeBitmap(env, ColorType::RGBA_8888, AlphaType::Premultiplied, tryHardware);
  if (tryHardware) {
    auto hardwareBuffer = HardwareBuffer::MakeFrom(env, nativeImage.get());
    if (hardwareBuffer != nullptr) {
      return hardwareBuffer;
    }
    bitmap = ConvertHardwareBitmap(env, bitmap);
  }
  auto imageBuffer = NativeImageBuffer::MakeFrom(env, bitmap);
  if (imageBuffer != nullptr) {
    return imageBuffer;
  }
  return ImageCodec::onMakeBuffer(tryHardware);
}

jobject NativeCodec::decodeBitmap(JNIEnv* env, ColorType colorType, AlphaType alphaType,
                                  bool tryHardware) const {
  if (!nativeImage.isEmpty()) {
    if (!tryHardware) {
      return ConvertHardwareBitmap(env, nativeImage.get());
    }
    return nativeImage.get();
  }
  auto options = env->NewObject(BitmapFactoryOptionsClass.get(), BitmapFactoryOptions_Constructor);
  if (options == nullptr) {
    env->ExceptionClear();
    LOGE("NativeCodec::decodeBitmap() Failed to create a BitmapFactory.Options object!");
    return nullptr;
  }
  jobject config;
  if (tryHardware && HardwareBufferInterface::HasBitmapFetchSupport()) {
    config = env->GetStaticObjectField(BitmapConfigClass.get(), BitmapConfig_HARDWARE);
  } else if (colorType == ColorType::ALPHA_8) {
    config = env->GetStaticObjectField(BitmapConfigClass.get(), BitmapConfig_ALPHA_8);
  } else if (colorType == ColorType::RGB_565) {
    config = env->GetStaticObjectField(BitmapConfigClass.get(), BitmapConfig_RGB_565);
  } else {
    config = env->GetStaticObjectField(BitmapConfigClass.get(), BitmapConfig_ARGB_8888);
  }
  env->SetObjectField(options, BitmapFactoryOptions_inPreferredConfig, config);
  if (alphaType == AlphaType::Unpremultiplied) {
    env->SetBooleanField(options, BitmapFactoryOptions_inPremultiplied, false);
  }

  if (!imagePath.empty()) {
    auto filePath = SafeToJString(env, imagePath);
    auto bitmap = env->CallStaticObjectMethod(BitmapFactoryClass.get(), BitmapFactory_decodeFile,
                                              filePath, options);
    if (env->ExceptionCheck()) {
      LOGE("NativeCodec::decodeBitmap() Failed to decode a Bitmap from the path: %s!",
           imagePath.c_str());
      return nullptr;
    }
    return bitmap;
  }
  auto byteArray = env->NewByteArray(imageBytes->size());
  env->SetByteArrayRegion(byteArray, 0, imageBytes->size(),
                          reinterpret_cast<const jbyte*>(imageBytes->data()));
  auto bitmap = env->CallStaticObjectMethod(BitmapFactoryClass.get(), BitmapFactory_decodeByteArray,
                                            byteArray, 0, imageBytes->size(), options);
  if (env->ExceptionCheck()) {
    LOGE("NativeCodec::decodeBitmap() Failed to decode a Bitmap from the image bytes!");
    return nullptr;
  }
  return bitmap;
}
}  // namespace tgfx
