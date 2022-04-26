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

#include "core/images/jpeg/JpegImage.h"
#include <csetjmp>
#include "core/utils/OrientationHelper.h"
#include "tgfx/core/Bitmap.h"

extern "C" {
#include "jerror.h"
#include "jpeglib.h"
}

namespace tgfx {

bool JpegImage::IsJpeg(const std::shared_ptr<Data>& data) {
  constexpr uint8_t jpegSig[] = {0xFF, 0xD8, 0xFF};
  return data->size() >= 3 && !memcmp(data->bytes(), jpegSig, sizeof(jpegSig));
}

const uint32_t kExifHeaderSize = 14;
const uint32_t kExifMarker = JPEG_APP0 + 1;

static bool is_orientation_marker(jpeg_marker_struct* marker, Orientation* orientation) {
  if (kExifMarker != marker->marker || marker->data_length < kExifHeaderSize) {
    return false;
  }

  constexpr uint8_t kExifSig[]{'E', 'x', 'i', 'f', '\0'};
  if (memcmp(marker->data, kExifSig, sizeof(kExifSig)) != 0) {
    return false;
  }

  // Account for 'E', 'x', 'i', 'f', '\0', '<fill byte>'.
  constexpr size_t kOffset = 6;
  return is_orientation_marker(marker->data + kOffset, marker->data_length - kOffset, orientation);
}

static Orientation get_exif_orientation(jpeg_decompress_struct* dinfo) {
  Orientation orientation;
  for (jpeg_marker_struct* marker = dinfo->marker_list; marker; marker = marker->next) {
    if (is_orientation_marker(marker, &orientation)) {
      return orientation;
    }
  }
  return Orientation::TopLeft;
}

struct my_error_mgr {
  struct jpeg_error_mgr pub;
  jmp_buf setjmp_buffer;
};

std::shared_ptr<Image> JpegImage::MakeFrom(const std::string& filePath) {
  return MakeFromData(filePath, nullptr);
}

std::shared_ptr<Image> JpegImage::MakeFrom(std::shared_ptr<Data> imageBytes) {
  return MakeFromData("", std::move(imageBytes));
}

std::shared_ptr<Image> JpegImage::MakeFromData(const std::string& filePath,
                                               std::shared_ptr<Data> byteData) {
  FILE* infile = nullptr;
  if (byteData == nullptr && (infile = fopen(filePath.c_str(), "rb")) == nullptr) {
    return nullptr;
  }
  jpeg_decompress_struct cinfo = {};
  my_error_mgr jerr = {};
  cinfo.err = jpeg_std_error(&jerr.pub);
  Orientation orientation = Orientation::TopLeft;
  do {
    if (setjmp(jerr.setjmp_buffer)) break;
    jpeg_create_decompress(&cinfo);
    if (infile) {
      jpeg_stdio_src(&cinfo, infile);
    } else {
      jpeg_mem_src(&cinfo, byteData->bytes(), byteData->size());
    }
    jpeg_save_markers(&cinfo, kExifMarker, 0xFFFF);
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) break;
    orientation = get_exif_orientation(&cinfo);
  } while (false);
  jpeg_destroy_decompress(&cinfo);
  if (infile) fclose(infile);
  if (cinfo.image_width == 0 || cinfo.image_height == 0) {
    return nullptr;
  }
  return std::shared_ptr<Image>(new JpegImage(static_cast<int>(cinfo.image_width),
                                              static_cast<int>(cinfo.image_height), orientation,
                                              filePath, std::move(byteData)));
}

bool JpegImage::readPixels(const ImageInfo& dstInfo, void* dstPixels) const {
  if (dstPixels == nullptr || dstInfo.isEmpty()) {
    return false;
  }
  if (dstInfo.colorType() == ColorType::ALPHA_8) {
    memset(dstPixels, 255, dstInfo.rowBytes() * height());
    return true;
  }
  FILE* infile = nullptr;
  if (fileData == nullptr && (infile = fopen(filePath.c_str(), "rb")) == nullptr) {
    return false;
  }
  jpeg_decompress_struct cinfo = {};
  my_error_mgr jerr = {};
  cinfo.err = jpeg_std_error(&jerr.pub);
  bool readRes = false;
  do {
    if (setjmp(jerr.setjmp_buffer)) break;
    jpeg_create_decompress(&cinfo);
    if (infile) {
      jpeg_stdio_src(&cinfo, infile);
    } else {
      jpeg_mem_src(&cinfo, fileData->bytes(), fileData->size());
    }
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) break;
    if (dstInfo.colorType() == ColorType::RGBA_8888) {
      cinfo.out_color_space = JCS_EXT_RGBA;
    } else if (dstInfo.colorType() == ColorType::BGRA_8888) {
      cinfo.out_color_space = JCS_EXT_BGRA;
    }
    if (!jpeg_start_decompress(&cinfo)) break;
    JSAMPROW pRow[1];
    int line = 0;
    JDIMENSION h = height();
    while (cinfo.output_scanline < h) {
      pRow[0] = (JSAMPROW)(static_cast<unsigned char*>(dstPixels) + dstInfo.rowBytes() * line);
      jpeg_read_scanlines(&cinfo, pRow, 1);
      line++;
    }
    readRes = jpeg_finish_decompress(&cinfo);
  } while (false);
  jpeg_destroy_decompress(&cinfo);
  if (infile) {
    fclose(infile);
  }
  return readRes;
}

#ifdef TGFX_USE_JPEG_ENCODE
std::shared_ptr<Data> JpegImage::Encode(const ImageInfo& imageInfo, const void* pixels,
                                        EncodedFormat, int quality) {
  auto srcPixels = static_cast<uint8_t*>(const_cast<void*>(pixels));
  if (imageInfo.colorType() == ColorType::ALPHA_8) {
    return nullptr;
  }
  jpeg_compress_struct cinfo = {};
  jpeg_error_mgr jerr = {};
  JSAMPROW row_pointer[1];
  int row_stride;
  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_compress(&cinfo);
  uint8_t* dstBuffer = nullptr;
  unsigned long dstBufferSize = 0;  // NOLINT
  jpeg_mem_dest(&cinfo, &dstBuffer, &dstBufferSize);
  cinfo.image_width = imageInfo.width();
  cinfo.image_height = imageInfo.height();
  cinfo.input_components = 4;
  switch (imageInfo.colorType()) {
    case ColorType::RGBA_8888:
    case ColorType::ALPHA_8:
      cinfo.in_color_space = JCS_EXT_RGBA;
      break;
    case ColorType::BGRA_8888:
      cinfo.in_color_space = JCS_EXT_BGRA;
      break;
    default:
      return nullptr;
  }
  jpeg_set_defaults(&cinfo);
  cinfo.optimize_coding = TRUE;
  jpeg_set_quality(&cinfo, quality, TRUE);
  jpeg_start_compress(&cinfo, TRUE);
  row_stride = imageInfo.width() * 4;
  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &(srcPixels)[cinfo.next_scanline * row_stride];
    (void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* similar to read file, clean up after we're done compressing */
  jpeg_finish_compress(&cinfo);
  jpeg_destroy_compress(&cinfo);
  return Data::MakeAdopted(dstBuffer, dstBufferSize, Data::FreeProc);
}
#endif

}  // namespace tgfx