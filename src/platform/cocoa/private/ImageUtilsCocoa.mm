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

#include "ImageUtilsCocoa.h"
#include "raster/coregraphics/BitmapContextUtil.h"

namespace pag {

CGAffineTransform EncodedOriginToCGAffineTransform(CGImagePropertyOrientation orientation,
                                                   int* width, int* height) {
  CGAffineTransform transform = CGAffineTransformIdentity;

  switch (orientation) {
    case kCGImagePropertyOrientationUp:
      break;
    case kCGImagePropertyOrientationDown:
      transform = CGAffineTransformMake(-1, 0, 0, -1, *width, *height);
      break;
    case kCGImagePropertyOrientationLeft:
      std::swap(*width, *height);
      transform = CGAffineTransformMake(0, 1, -1, 0, *width, 0);
      break;
    case kCGImagePropertyOrientationRight:
      std::swap(*width, *height);
      transform = CGAffineTransformMake(0, -1, 1, 0, 0, *height);
      break;
    case kCGImagePropertyOrientationUpMirrored:
      transform = CGAffineTransformMake(-1, 0, 0, 1, *width, 0);
      break;
    case kCGImagePropertyOrientationDownMirrored:
      transform = CGAffineTransformMake(1, 0, 0, -1, 0, *height);
      break;
    case kCGImagePropertyOrientationLeftMirrored:
      std::swap(*width, *height);
      transform = CGAffineTransformMake(0, -1, -1, 0, *width, *height);
      break;
    case kCGImagePropertyOrientationRightMirrored:
      std::swap(*width, *height);
      transform = CGAffineTransformMake(0, 1, 1, 0, 0, 0);
      break;

    default:
      break;
  }

  return transform;
}

bool ReadPixelsFromCGImage(CGImageRef cgImage, const ImageInfo& dstInfo, void* resultPixels,
                           CGImagePropertyOrientation orientation) {
  int width = static_cast<int>(CGImageGetWidth(cgImage));
  int height = static_cast<int>(CGImageGetHeight(cgImage));
  auto info =
      ImageInfo::Make(width, height, dstInfo.colorType(), dstInfo.alphaType(), dstInfo.rowBytes());
  auto context = CreateBitmapContext(info, resultPixels);
  if (context == nullptr) {
    return false;
  }
  CGAffineTransform transform = EncodedOriginToCGAffineTransform(orientation, &width, &height);
  CGContextConcatCTM(context, transform);
  CGRect rect = CGRectMake(0, 0, CGImageGetWidth(cgImage), CGImageGetHeight(cgImage));
  CGContextSetBlendMode(context, kCGBlendModeCopy);
  CGContextDrawImage(context, rect, cgImage);
  CGContextRelease(context);
  return true;
}
}
