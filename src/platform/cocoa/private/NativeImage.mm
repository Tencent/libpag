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
#include "ImageUtilsCocoa.h"
#include "raster/coregraphics/BitmapContextUtil.h"

namespace pag {
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

static CGSize GetImageSize(CGImageSourceRef imageSource) {
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
    CGImagePropertyOrientation orientation = GetOrientationFromProperties(imageProperties);
    switch (orientation) {
      case kCGImagePropertyOrientationLeftMirrored:
      case kCGImagePropertyOrientationRight:
      case kCGImagePropertyOrientationRightMirrored:
      case kCGImagePropertyOrientationLeft:
        std::swap(width, height);
        break;

      default:
        break;
    }
    CFRelease(imageProperties);
  }
  return CGSizeMake(width, height);
}

std::unique_ptr<Image> NativeImage::MakeFrom(const std::string& filePath) {
  CFStringRef imagePath = CFStringCreateWithCString(NULL, filePath.c_str(), kCFStringEncodingUTF8);
  CFURLRef imageURL = CFURLCreateWithFileSystemPath(NULL, imagePath, kCFURLPOSIXPathStyle, 0);
  CFRelease(imagePath);
  CGImageSourceRef imageSource = CGImageSourceCreateWithURL(imageURL, NULL);
  CFRelease(imageURL);
  if (imageSource == nil) {
    return nullptr;
  }
  auto size = GetImageSize(imageSource);
  CFRelease(imageSource);
  if (size.width <= 0 || size.height <= 0) {
    return nullptr;
  }
  auto image = new NativeImage(size.width, size.height);
  image->imagePath = filePath;
  return std::unique_ptr<Image>(image);
}

std::unique_ptr<Image> NativeImage::MakeFrom(std::shared_ptr<Data> imageBytes) {
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
  auto size = GetImageSize(imageSource);
  CFRelease(imageSource);
  if (size.width <= 0 || size.height <= 0) {
    return nullptr;
  }
  auto image = new NativeImage(size.width, size.height);
  image->imageBytes = imageBytes;
  return std::unique_ptr<Image>(image);
}

bool NativeImage::readPixels(const ImageInfo& dstInfo, void* dstPixels) const {
  if (dstInfo.isEmpty() || dstPixels == nullptr) {
    return false;
  }
  NSData* data = nil;
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
  CGImageSourceRef sourceRef = CGImageSourceCreateWithData(dataRef, NULL);
  if (sourceRef == NULL) {
    [data release];
    return false;
  }

  CGImageRef cgImage = CGImageSourceCreateImageAtIndex(sourceRef, 0, NULL);
  if (cgImage == NULL) {
    [data release];
    CFRelease(sourceRef);
    return false;
  }

  CFDictionaryRef imageProperties = CGImageSourceCopyPropertiesAtIndex(sourceRef, 0, NULL);
  CGImagePropertyOrientation orientation = GetOrientationFromProperties(imageProperties);

  auto result = pag::ReadPixelsFromCGImage(cgImage, dstInfo, dstPixels, orientation);
  [data release];
  CFRelease(sourceRef);
  CFRelease(imageProperties);
  CGImageRelease(cgImage);
  return result;
}
}
