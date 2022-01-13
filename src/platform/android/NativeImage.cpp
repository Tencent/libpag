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

#include "JNIHelper.h"
#include "image/PixelMap.h"

namespace pag {
#define COLOR_TYPE_ALPHA_8 1
#define COLOR_TYPE_ARGB_8888_PREMULTIPLIED 2
#define COLOR_TYPE_ARGB_8888_UNPREMULTIPLIED 3

static Global<jclass> ImageCodecClass;
static jmethodID ImageCodec_GetOrientationFromPath;
static jmethodID ImageCodec_GetOrientationFromBytes;
static jmethodID ImageCodec_GetSizeFromPath;
static jmethodID ImageCodec_GetSizeFromBytes;
static jmethodID ImageCodec_CreateBitmapFromPath;
static jmethodID ImageCodec_CreateBitmapFromBytes;

void NativeImage::InitJNI(JNIEnv* env) {
  ImageCodecClass.reset(env, env->FindClass("org/libpag/ImageCodec"));
  ImageCodec_GetSizeFromPath =
      env->GetStaticMethodID(ImageCodecClass.get(), "GetSizeFromPath", "(Ljava/lang/String;)[I");
  ImageCodec_GetSizeFromBytes = env->GetStaticMethodID(ImageCodecClass.get(), "GetSizeFromBytes",
                                                       "(Ljava/nio/ByteBuffer;)[I");
  ImageCodec_GetOrientationFromPath = env->GetStaticMethodID(
      ImageCodecClass.get(), "GetOrientationFromPath", "(Ljava/lang/String;)I");
  ImageCodec_GetOrientationFromBytes = env->GetStaticMethodID(
      ImageCodecClass.get(), "GetOrientationFromBytes", "(Ljava/nio/ByteBuffer;)I");
  ImageCodec_CreateBitmapFromPath =
      env->GetStaticMethodID(ImageCodecClass.get(), "CreateBitmapFromPath",
                             "(Ljava/lang/String;I)Landroid/graphics/Bitmap;");
  ImageCodec_CreateBitmapFromBytes =
      env->GetStaticMethodID(ImageCodecClass.get(), "CreateBitmapFromBytes",
                             "(Ljava/nio/ByteBuffer;I)Landroid/graphics/Bitmap;");
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

std::shared_ptr<Image> NativeImage::MakeFrom(const std::string& filePath) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return nullptr;
  }
  jstring imagePath = SafeConvertToJString(env, filePath.c_str());
  auto sizeObject =
      env->CallStaticObjectMethod(ImageCodecClass.get(), ImageCodec_GetSizeFromPath, imagePath);
  auto orientation =
      env->CallStaticIntMethod(ImageCodecClass.get(), ImageCodec_GetOrientationFromPath, imagePath);
  auto image = Make(env, sizeObject, orientation);
  if (image != nullptr) {
    image->imagePath = filePath;
  }
  env->DeleteLocalRef(sizeObject);
  env->DeleteLocalRef(imagePath);
  return image;
}

std::shared_ptr<Image> NativeImage::MakeFrom(std::shared_ptr<Data> imageBytes) {
  auto env = JNIEnvironment::Current();
  if (env == nullptr || imageBytes == nullptr) {
    return nullptr;
  }
  auto byteBuffer =
      env->NewDirectByteBuffer(const_cast<void*>(imageBytes->data()), imageBytes->size());
  auto sizeObject =
      env->CallStaticObjectMethod(ImageCodecClass.get(), ImageCodec_GetSizeFromBytes, byteBuffer);
  auto orientation = env->CallStaticIntMethod(ImageCodecClass.get(),
                                              ImageCodec_GetOrientationFromBytes, byteBuffer);
  auto image = Make(env, sizeObject, orientation);
  if (image != nullptr) {
    image->imageBytes = imageBytes;
  }
  env->DeleteLocalRef(sizeObject);
  env->DeleteLocalRef(byteBuffer);
  return image;
}

bool NativeImage::readPixels(const ImageInfo& dstInfo, void* dstPixels) const {
  if (dstInfo.isEmpty() || dstPixels == nullptr) {
    return false;
  }
  auto env = JNIEnvironment::Current();
  if (env == nullptr) {
    return false;
  }
  int androidColorType = 0;
  switch (dstInfo.colorType()) {
    case ColorType::RGBA_8888:
      androidColorType = dstInfo.alphaType() == AlphaType::Premultiplied
                             ? COLOR_TYPE_ARGB_8888_PREMULTIPLIED
                             : COLOR_TYPE_ARGB_8888_UNPREMULTIPLIED;
      break;
    case ColorType::ALPHA_8:
      androidColorType = COLOR_TYPE_ALPHA_8;
      break;
    default:
      break;
  }
  if (androidColorType == 0) {
    return false;
  }
  jobject bitmap = NULL;
  if (!imagePath.empty()) {
    jstring path = SafeConvertToJString(env, imagePath.c_str());
    bitmap = env->CallStaticObjectMethod(ImageCodecClass.get(), ImageCodec_CreateBitmapFromPath,
                                         path, androidColorType);
    env->DeleteLocalRef(path);
  } else {
    auto byteBuffer =
        env->NewDirectByteBuffer(const_cast<void*>(imageBytes->data()), imageBytes->size());
    bitmap = env->CallStaticObjectMethod(ImageCodecClass.get(), ImageCodec_CreateBitmapFromBytes,
                                         byteBuffer, androidColorType);
    env->DeleteLocalRef(byteBuffer);
  }
  auto info = GetImageInfo(env, bitmap);
  if (info.isEmpty()) {
    env->DeleteLocalRef(bitmap);
    return false;
  }
  void* pixels = nullptr;
  if (AndroidBitmap_lockPixels(env, bitmap, &pixels) != 0) {
    env->DeleteLocalRef(bitmap);
    return false;
  }
  PixelMap pixelMap(info, pixels);
  auto result = pixelMap.readPixels(dstInfo, dstPixels);
  AndroidBitmap_unlockPixels(env, bitmap);
  env->DeleteLocalRef(bitmap);
  return result;
}
}  // namespace pag
