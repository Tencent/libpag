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

#include "core/images/png/PngImage.h"
#include "png.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Buffer.h"

namespace tgfx {
std::shared_ptr<Image> PngImage::MakeFrom(const std::string& filePath) {
  return MakeFromData(filePath, nullptr);
}

std::shared_ptr<Image> PngImage::MakeFrom(std::shared_ptr<Data> imageBytes) {
  return MakeFromData("", std::move(imageBytes));
}

bool PngImage::IsPng(const std::shared_ptr<Data>& data) {
  constexpr png_byte png_signature[8] = {137, 80, 78, 71, 13, 10, 26, 10};
  return data->size() >= 8 && !memcmp(data->bytes(), &png_signature[0], 8);
}

typedef struct {
  const unsigned char* base;
  const unsigned char* end;
  const unsigned char* cursor;
} PngReader;

static void PngReaderReadData(png_structp png_ptr, png_bytep data, png_size_t length) {
  auto reader = static_cast<PngReader*>(png_get_io_ptr(png_ptr));
  auto avail = static_cast<png_size_t>(reader->end - reader->cursor);
  if (avail > length) {
    avail = length;
  }
  memcpy(data, reader->cursor, avail);
  reader->cursor += avail;
}

class ReadInfo {
 public:
  static std::shared_ptr<ReadInfo> Make(const std::string& filePath,
                                        const std::shared_ptr<Data>& fileData) {
    auto readInfo = std::make_shared<ReadInfo>();
    if (!filePath.empty()) {
      readInfo->infile = fopen(filePath.c_str(), "rb");
    }
    if (fileData == nullptr && readInfo->infile == nullptr) {
      return nullptr;
    }
    readInfo->p = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (readInfo->p == nullptr) {
      return nullptr;
    }
    readInfo->pi = png_create_info_struct(readInfo->p);
    if (readInfo->pi == nullptr) {
      return nullptr;
    }
#ifdef PNG_SET_OPTION_SUPPORTED
    png_set_option(readInfo->p, PNG_MAXIMUM_INFLATE_WINDOW, PNG_OPTION_ON);
#endif
    if (setjmp(png_jmpbuf(readInfo->p))) {
      return nullptr;
    }
    if (readInfo->infile) {
      png_init_io(readInfo->p, readInfo->infile);
    } else {
      readInfo->reader = new PngReader();
      readInfo->reader->base = fileData->bytes();
      readInfo->reader->end = fileData->bytes() + fileData->size();
      readInfo->reader->cursor = fileData->bytes();
      png_set_read_fn(readInfo->p, readInfo->reader, PngReaderReadData);
    }
    png_read_info(readInfo->p, readInfo->pi);
    return readInfo;
  }

  ~ReadInfo() {
    if (p) {
      png_destroy_read_struct(&p, &pi, nullptr);
    }
    if (data) {
      free(data);
    }
    if (rowPtrs) {
      free(rowPtrs);
    }
    if (infile) {
      fclose(infile);
    }
    delete reader;
  }

  png_structp p = nullptr;
  png_infop pi = nullptr;
  FILE* infile = nullptr;
  PngReader* reader = nullptr;
  unsigned char** rowPtrs = nullptr;
  unsigned char* data = nullptr;
};

std::shared_ptr<Image> PngImage::MakeFromData(const std::string& filePath,
                                              std::shared_ptr<Data> byteData) {
  auto readInfo = ReadInfo::Make(filePath, byteData);
  if (readInfo == nullptr) {
    return nullptr;
  }
  png_uint_32 w = 0, h = 0;
  png_get_IHDR(readInfo->p, readInfo->pi, &w, &h, nullptr, nullptr, nullptr, nullptr, nullptr);
  if (w == 0 || h == 0) {
    return nullptr;
  }
  return std::shared_ptr<Image>(new PngImage(static_cast<int>(w), static_cast<int>(h),
                                             Orientation::TopLeft, filePath, std::move(byteData)));
}

static void UpdateReadInfo(png_structp p, png_infop pi) {
  int originalColorType = png_get_color_type(p, pi);
  int bitDepth = png_get_bit_depth(p, pi);
  if (bitDepth == 16) {
    png_set_strip_16(p);
  }
  if (originalColorType == PNG_COLOR_TYPE_PALETTE) {
    png_set_palette_to_rgb(p);
  }
  if (originalColorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
    png_set_expand_gray_1_2_4_to_8(p);
  }
  if (png_get_valid(p, pi, PNG_INFO_tRNS)) {
    png_set_tRNS_to_alpha(p);
  }
  if (originalColorType == PNG_COLOR_TYPE_RGB || originalColorType == PNG_COLOR_TYPE_GRAY ||
      originalColorType == PNG_COLOR_TYPE_PALETTE) {
    png_set_filler(p, 0xFF, PNG_FILLER_AFTER);
  }
  if (originalColorType == PNG_COLOR_TYPE_GRAY || originalColorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
    png_set_gray_to_rgb(p);
  }
  png_read_update_info(p, pi);
}

bool PngImage::readPixels(const ImageInfo& dstInfo, void* dstPixels) const {
  auto readInfo = ReadInfo::Make(filePath, fileData);
  if (readInfo == nullptr) {
    return false;
  }
  int w = width();
  int h = height();
  if (h == 0 || w == 0) {
    return false;
  }
  UpdateReadInfo(readInfo->p, readInfo->pi);
  readInfo->rowPtrs = (unsigned char**)malloc(sizeof(unsigned char*) * h);
  if (dstInfo.colorType() != ColorType::RGBA_8888 ||
      dstInfo.alphaType() != AlphaType::Unpremultiplied) {
    readInfo->data = (unsigned char*)malloc((w * 4) * h);
    for (int i = 0; i < h; i++) {
      readInfo->rowPtrs[i] = readInfo->data + ((w * 4) * i);
    }
    png_read_image(readInfo->p, readInfo->rowPtrs);
    if (readInfo->data == nullptr || readInfo->rowPtrs == nullptr) {
      return false;
    }
    ImageInfo info = ImageInfo::Make(w, h, ColorType::RGBA_8888, AlphaType::Unpremultiplied);
    Bitmap bitmap(info, readInfo->data);
    return bitmap.readPixels(dstInfo, dstPixels);
  } else {
    if (readInfo->rowPtrs == nullptr) return false;
    for (int i = 0; i < h; i++) {
      readInfo->rowPtrs[i] = static_cast<unsigned char*>(dstPixels) + ((w * 4) * i);
    }
    png_read_image(readInfo->p, readInfo->rowPtrs);
    return true;
  }
}

#ifdef TGFX_USE_PNG_ENCODE
struct PngWriter {
  unsigned char* data = nullptr;
  size_t length = 0;
};

static void png_reader_write_data(png_structp png_ptr, png_bytep data, png_size_t length) {
  auto writer = static_cast<PngWriter*>(png_get_io_ptr(png_ptr));
  size_t nsize = writer->length + length;
  if (writer->data) {
    writer->data = static_cast<unsigned char*>(realloc(writer->data, nsize));
  } else {
    writer->data = static_cast<unsigned char*>(malloc(nsize));
  }
  memcpy(writer->data + writer->length, data, length);
  writer->length += length;
}

std::shared_ptr<Data> PngImage::Encode(const ImageInfo& imageInfo, const void* pixels,
                                       EncodedFormat, int) {
  auto srcPixels = static_cast<png_bytep>(const_cast<void*>((pixels)));
  uint8_t* alphaPixels = nullptr;
  std::unique_ptr<Buffer> tempPixels = nullptr;
  if (imageInfo.colorType() == ColorType::ALPHA_8) {
    tempPixels = std::make_unique<Buffer>(imageInfo.width() * 2);
    alphaPixels = tempPixels->bytes();
  } else if (imageInfo.colorType() != ColorType::RGBA_8888 ||
             imageInfo.alphaType() != AlphaType::Unpremultiplied) {
    Bitmap bitmap(imageInfo, srcPixels);
    tempPixels = std::make_unique<Buffer>(imageInfo.byteSize());
    auto dstInfo = ImageInfo::Make(imageInfo.width(), imageInfo.height(), ColorType::RGBA_8888,
                                   AlphaType::Unpremultiplied);
    if (!bitmap.readPixels(dstInfo, tempPixels->data())) {
      return nullptr;
    }
    srcPixels = tempPixels->bytes();
  }
  PngWriter pngWriter;
  png_structp png_ptr = nullptr;
  png_infop info_ptr = nullptr;
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  bool encodeSuccess = false;
  do {
    if (png_ptr == nullptr) {
      break;
    }
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == nullptr) {
      break;
    }
    if (setjmp(png_jmpbuf(png_ptr))) {
      break;
    }
    png_color_8 sigBit = {8, 8, 8, 0, 8};
    int colorType = PNG_COLOR_TYPE_RGB_ALPHA;
    if (imageInfo.colorType() == ColorType::ALPHA_8) {
      // We store ALPHA_8 images as GrayAlpha in png. Our private signal is significant bits for
      // gray.
      // If that is set to 1, we assume the gray channel can be ignored, and we output just alpha.
      // We tried 0 at first, but png doesn't like a 0 sigbit for a channel it expects, hence we
      // chose 1.
      sigBit = {0, 0, 0, 1, 8};
      colorType = PNG_COLOR_TYPE_GRAY_ALPHA;
    }
    png_set_IHDR(png_ptr, info_ptr, imageInfo.width(), imageInfo.height(), 8, colorType,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_set_sBIT(png_ptr, info_ptr, &sigBit);
    png_set_write_fn(png_ptr, &pngWriter, png_reader_write_data, nullptr);
    png_write_info(png_ptr, info_ptr);
    int size = sizeof(uint8_t);
    if (size < 0) {
      return nullptr;
    }
    for (int i = 0; i < imageInfo.height(); ++i) {
      if (imageInfo.colorType() == ColorType::ALPHA_8) {
        // convert alpha8 to gray
        for (int j = 0; j < imageInfo.width(); j++) {
          *(alphaPixels++) = 0;
          *(alphaPixels++) = *(srcPixels++);
        }
        alphaPixels -= imageInfo.width() * 2;
        png_write_row(png_ptr, reinterpret_cast<png_const_bytep>(alphaPixels));
      } else {
        png_write_row(png_ptr, reinterpret_cast<png_const_bytep>(srcPixels));
        srcPixels += imageInfo.width() * 4;
      }
    }
    png_write_end(png_ptr, info_ptr);
    encodeSuccess = true;
  } while (false);
  if (png_ptr) {
    png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
  }
  if (!encodeSuccess) {
    if (pngWriter.data) {
      free(pngWriter.data);
    }
    return nullptr;
  }
  return Data::MakeAdopted(pngWriter.data, pngWriter.length, Data::FreeProc);
}
#endif

}  // namespace tgfx
