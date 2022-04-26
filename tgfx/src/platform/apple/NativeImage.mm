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
#include "BitmapContextUtil.h"
#include "platform/NativeCodec.h"

namespace tgfx {
static CGImagePropertyOrientation GetOrientationFromProperties(CFDictionaryRef imageProperties) {
  CGImagePropertyOrientation orientation = kCGImagePropertyOrientationUp;
  if (imageProperties != NULL) {
    CFNumberRef oritentationNum =
        (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyOrientation);
    if (oritentationNum != NULL) {
      CFNumberGetValue(oritentationNum, kCFNumberIntType, &orientation);
    }
  }

  return orientation;
}

static CGSize GetImageSize(CGImageSourceRef imageSource, CGImagePropertyOrientation* orientation) {
  int width = 0;
  int height = 0;
  CFDictionaryRef imageProperties = CGImageSourceCopyPropertiesAtIndex(imageSource, 0, NULL);
  if (imageProperties != NULL) {
    CFNumberRef widthNum =
        (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyPixelWidth);
    if (widthNum != NULL) {
      CFNumberGetValue(widthNum, kCFNumberIntType, &width);
    }
    CFNumberRef heightNum =
        (CFNumberRef)CFDictionaryGetValue(imageProperties, kCGImagePropertyPixelHeight);
    if (heightNum != NULL) {
      CFNumberGetValue(heightNum, kCFNumberIntType, &height);
    }
    *orientation = GetOrientationFromProperties(imageProperties);
    CFRelease(imageProperties);
  }
  return CGSizeMake(width, height);
}

std::shared_ptr<Image> NativeCodec::MakeImage(const std::string& filePath) {
  CFStringRef imagePath = CFStringCreateWithCString(NULL, filePath.c_str(), kCFStringEncodingUTF8);
  CFURLRef imageURL = CFURLCreateWithFileSystemPath(NULL, imagePath, kCFURLPOSIXPathStyle, 0);
  CFRelease(imagePath);
  CGImageSourceRef imageSource = CGImageSourceCreateWithURL(imageURL, NULL);
  CFRelease(imageURL);
  if (imageSource == nil) {
    return nullptr;
  }
  CGImagePropertyOrientation orientation = kCGImagePropertyOrientationUp;
  auto size = GetImageSize(imageSource, &orientation);
  CFRelease(imageSource);
  if (size.width <= 0 || size.height <= 0) {
    return nullptr;
  }
  auto image = new NativeImage(size.width, size.height, static_cast<Orientation>(orientation));
  image->imagePath = filePath;
  return std::unique_ptr<Image>(image);
}

std::shared_ptr<Image> NativeCodec::MakeImage(std::shared_ptr<Data> imageBytes) {
  if (imageBytes == nullptr) {
    return nullptr;
  }
  CFDataRef data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault,
                                               reinterpret_cast<const UInt8*>(imageBytes->data()),
                                               imageBytes->size(), kCFAllocatorNull);
  if (data == nullptr) {
    return nullptr;
  }
  CGImageSourceRef imageSource = CGImageSourceCreateWithData(data, NULL);
  if (imageSource == nil) {
    return nullptr;
  }
  CGImagePropertyOrientation orientation = kCGImagePropertyOrientationUp;
  auto size = GetImageSize(imageSource, &orientation);
  CFRelease(imageSource);
  if (size.width <= 0 || size.height <= 0) {
    return nullptr;
  }
  auto image = new NativeImage(size.width, size.height, static_cast<Orientation>(orientation));
  image->imageBytes = imageBytes;
  return std::unique_ptr<Image>(image);
}

std::shared_ptr<Image> NativeCodec::MakeFrom(void* nativeImage) {
  auto cgImage = reinterpret_cast<CGImageRef>(nativeImage);
  auto width = CGImageGetWidth(cgImage);
  auto height = CGImageGetHeight(cgImage);
  if (width <= 0 || height <= 0) {
    return nullptr;
  }
  auto image = new NativeImage(static_cast<int>(width), static_cast<int>(height), Orientation::TopLeft);
  CFRetain(cgImage);
  image->cgImage = cgImage;
  return std::unique_ptr<Image>(image);
}


NativeImage::~NativeImage() {
  if (cgImage != nullptr) {
    CFRelease(cgImage);
  }
}

bool NativeImage::readPixels(const ImageInfo& dstInfo, void* dstPixels) const {
  if (dstInfo.isEmpty() || dstPixels == nullptr) {
    return false;
  }
  NSData* data = nil;
  CGImageSourceRef sourceRef = nil;
  CGImageRef image = nil;
  if (cgImage != nullptr) {
    image = cgImage;
  } else {
    if (!imagePath.empty()) {
      data =
          [[NSData alloc] initWithContentsOfFile:[NSString stringWithUTF8String:imagePath.c_str()]];
    } else {
      auto bytes = const_cast<void*>(imageBytes->data());
      data = [[NSData alloc] initWithBytesNoCopy:bytes length:imageBytes->size() freeWhenDone:NO];
    }
    if (data == nil) {
      return false;
    }
    CFDataRef dataRef = (__bridge CFDataRef)data;
    sourceRef = CGImageSourceCreateWithData(dataRef, NULL);
    if (sourceRef == NULL) {
      [data release];
      return false;
    }
    image = CGImageSourceCreateImageAtIndex(sourceRef, 0, NULL);
    if (image == NULL) {
      [data release];
      CFRelease(sourceRef);
      return false;
    }
  }
  auto context = CreateBitmapContext(dstInfo, dstPixels);
  auto result = context != nullptr;
  if (result) {
    int width = static_cast<int>(CGImageGetWidth(image));
    int height = static_cast<int>(CGImageGetHeight(image));
    CGRect rect = CGRectMake(0, 0, width, height);
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, rect, image);
    CGContextRelease(context);
  }
  if (cgImage == nullptr) {
    [data release];
    CFRelease(sourceRef);
    CGImageRelease(image);
  }
  return result;
}
}
