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

#include "core/images/webp/WebpImage.h"
#include "core/images/webp/WebpUtility.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/Buffer.h"

namespace tgfx {

bool WebpImage::IsWebp(const std::shared_ptr<Data>& data) {
  const char* bytes = static_cast<const char*>(data->data());
  return data->size() >= 14 && !memcmp(bytes, "RIFF", 4) && !memcmp(&bytes[8], "WEBPVP", 6);
}

std::shared_ptr<Image> WebpImage::MakeFrom(const std::string& filePath) {
  auto info = WebpUtility::getDecodeInfo(filePath);
  if (info.width == 0 || info.height == 0) {
    auto data = Data::MakeFromFile(filePath);
    info = WebpUtility::getDecodeInfo(data->data(), data->size());
    if (info.width == 0 || info.height == 0) {
      return nullptr;
    }
  }
  return std::shared_ptr<Image>(
      new WebpImage(info.width, info.height, info.orientation, filePath, nullptr));
}

std::shared_ptr<Image> WebpImage::MakeFrom(std::shared_ptr<Data> imageBytes) {
  if (imageBytes == nullptr) {
    return nullptr;
  }
  auto info = WebpUtility::getDecodeInfo(imageBytes->data(), imageBytes->size());
  if (info.width == 0 || info.height == 0) {
    return nullptr;
  }
  return std::shared_ptr<Image>(
      new WebpImage(info.width, info.height, info.orientation, "", std::move(imageBytes)));
}

static WEBP_CSP_MODE webp_decode_mode(ColorType dstCT, bool premultiply) {
  switch (dstCT) {
    case ColorType::BGRA_8888:
      return premultiply ? MODE_bgrA : MODE_BGRA;
    case ColorType::RGBA_8888:
      return premultiply ? MODE_rgbA : MODE_RGBA;
    default:
      return MODE_LAST;
  }
}

bool WebpImage::readPixels(const ImageInfo& dstInfo, void* dstPixels) const {
  if (dstPixels == nullptr || dstInfo.isEmpty()) {
    return false;
  }
  auto byteData = fileData;
  if (byteData == nullptr) {
    byteData = Data::MakeFromFile(filePath);
  }
  if (byteData == nullptr) {
    return false;
  }
  WebPDecoderConfig config;
  if (!WebPInitDecoderConfig(&config)) return false;
  if (WebPGetFeatures(byteData->bytes(), byteData->size(), &config.input) != VP8_STATUS_OK) {
    return false;
  }
  config.output.is_external_memory = 1;
  bool decodeSuccess = true;
  if (dstInfo.colorType() == ColorType::ALPHA_8) {
    // decode to RGBA_8888
    config.output.colorspace = webp_decode_mode(ColorType::RGBA_8888, false);
    config.output.u.RGBA.stride = width() * ImageInfo::GetBytesPerPixel(ColorType::RGBA_8888);
    config.output.u.RGBA.size = config.output.u.RGBA.stride * height();
    auto pixels = new (std::nothrow) uint8_t[config.output.u.RGBA.size];
    if (pixels) {
      config.output.u.RGBA.rgba = pixels;
      decodeSuccess = WebPDecode(byteData->bytes(), byteData->size(), &config) == VP8_STATUS_OK;
      // convert to ALPHA_8
      if (decodeSuccess) {
        auto info =
            ImageInfo::Make(width(), height(), ColorType::RGBA_8888, AlphaType::Unpremultiplied);
        Bitmap bitmap(info, pixels);
        decodeSuccess = bitmap.readPixels(dstInfo, dstPixels);
      }
      delete[] pixels;
    }
  } else {
    config.output.colorspace =
        webp_decode_mode(dstInfo.colorType(), dstInfo.alphaType() == AlphaType::Premultiplied);
    config.output.u.RGBA.rgba = reinterpret_cast<uint8_t*>(dstPixels);
    config.output.u.RGBA.stride = static_cast<int>(dstInfo.rowBytes());
    config.output.u.RGBA.size = dstInfo.rowBytes() * height();
    auto code = WebPDecode(byteData->bytes(), byteData->size(), &config);
    decodeSuccess = code == VP8_STATUS_OK;
  }
  WebPFreeDecBuffer(&config.output);
  return decodeSuccess;
}

#ifdef TGFX_USE_WEBP_ENCODE
struct WebpWriter {
  unsigned char* data = nullptr;
  size_t length = 0;
};

static int webp_reader_write_data(const uint8_t* data, size_t data_size,
                                  const WebPPicture* picture) {
  auto writer = static_cast<WebpWriter*>(picture->custom_ptr);
  size_t nsize = writer->length + data_size;
  if (writer->data) {
    writer->data = static_cast<unsigned char*>(realloc(writer->data, nsize));
  } else {
    writer->data = static_cast<unsigned char*>(malloc(nsize));
  }
  memcpy(writer->data + writer->length, data, data_size);
  writer->length += data_size;
  return 1;
}

std::shared_ptr<Data> WebpImage::Encode(const ImageInfo& imageInfo, const void* pixels,
                                        EncodedFormat, int quality) {
  const uint8_t* srcPixels = static_cast<uint8_t*>(const_cast<void*>(pixels));
  std::unique_ptr<Buffer> tempPixels = nullptr;
  if (imageInfo.alphaType() == AlphaType::Premultiplied ||
      imageInfo.colorType() == ColorType::ALPHA_8) {
    Bitmap bitmap(imageInfo, srcPixels);
    tempPixels = std::make_unique<Buffer>(imageInfo.width() * imageInfo.height() * 4);
    auto colorType =
        imageInfo.colorType() == ColorType::ALPHA_8 ? ColorType::RGBA_8888 : imageInfo.colorType();
    auto dstInfo = ImageInfo::Make(imageInfo.width(), imageInfo.height(), colorType,
                                   AlphaType::Unpremultiplied);
    if (!bitmap.readPixels(dstInfo, tempPixels->data())) {
      return nullptr;
    }
    srcPixels = tempPixels->bytes();
  }
  WebPConfig webp_config;
  bool isLossless = false;
  if (quality == 100) {
    quality = 75;
    isLossless = true;
  }
  if (!WebPConfigPreset(&webp_config, WEBP_PRESET_DEFAULT, static_cast<float>(quality))) {
    return nullptr;
  }
  WebPPicture pic;
  WebPPictureInit(&pic);
  pic.width = imageInfo.width();
  pic.height = imageInfo.height();
  pic.writer = webp_reader_write_data;
  if (isLossless) {
    webp_config.lossless = 1;
    webp_config.method = 0;
    pic.use_argb = 1;
  } else {
    webp_config.lossless = 0;
    webp_config.method = 3;
    pic.use_argb = 0;
  }

  WebpWriter webpWriter;
  pic.custom_ptr = &webpWriter;
  const int rgbStride = pic.width * 4;
  auto importProc = WebPPictureImportRGBX;
  if (ColorType::RGBA_8888 == imageInfo.colorType() ||
      ColorType::ALPHA_8 == imageInfo.colorType()) {
    if (AlphaType::Opaque == imageInfo.alphaType()) {
      importProc = WebPPictureImportRGBX;
    } else {
      importProc = WebPPictureImportRGBA;
    }
  } else if (ColorType::BGRA_8888 == imageInfo.colorType()) {
    if (AlphaType::Opaque == imageInfo.alphaType()) {
      importProc = WebPPictureImportBGRX;
    } else {
      importProc = WebPPictureImportBGRA;
    }
  }
  if (!importProc(&pic, &srcPixels[0], rgbStride) || !WebPEncode(&webp_config, &pic)) {
    WebPPictureFree(&pic);
    if (webpWriter.data) {
      free(webpWriter.data);
    }
    return nullptr;
  }
  WebPPictureFree(&pic);
  return Data::MakeAdopted(webpWriter.data, webpWriter.length, Data::FreeProc);
}
#endif

}  // namespace tgfx
