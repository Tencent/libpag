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

#include "WebSoftwareDecoderFactory.h"

using namespace emscripten;

namespace pag {
class WebSoftwareDecoder : public SoftwareDecoder {
 public:
  static std::unique_ptr<WebSoftwareDecoder> Make(val decoder) {
    if (!decoder.as<bool>()) {
      return nullptr;
    }
    return std::unique_ptr<WebSoftwareDecoder>(new WebSoftwareDecoder(std::move(decoder)));
  }

  bool onConfigure(const std::vector<HeaderData>& headerData, std::string mimeType, int width,
                   int height) override {
    auto array = val::array();
    for (const auto& data : headerData) {
      array.call<void>("push", val(typed_memory_view(data.length, data.data)));
    }
    return decoder.call<bool>("onConfigure", array, mimeType, width, height);
  }

  DecoderResult onSendBytes(void* bytes, size_t length, int64_t timestamp) override {
    return decoder.call<DecoderResult>(
        "onSendBytes", val(typed_memory_view(length, static_cast<uint8_t*>(bytes))), timestamp);
  }

  DecoderResult onDecodeFrame() override {
    return decoder.call<DecoderResult>("onDecodeFrame");
  }

  DecoderResult onEndOfStream() override {
    return decoder.call<DecoderResult>("onEndOfStream");
  }

  void onFlush() override {
    decoder.call<void>("onFlush");
  }

  std::unique_ptr<YUVBuffer> onRenderFrame() override {
    auto buffer = decoder.call<val>("onRenderFrame");
    if (!buffer.as<bool>()) {
      return nullptr;
    }
    auto data = buffer.call<val>("data");
    if (!data.isArray() || data.call<int>("length") != 3) {
      return nullptr;
    }
    auto lineSize = buffer.call<val>("lineSize");
    if (!lineSize.isArray() || lineSize.call<int>("length") != 3) {
      return nullptr;
    }
    auto yuvBuffer = std::make_unique<YUVBuffer>();
    for (int i = 0; i < 3; ++i) {
      yuvBuffer->data[i] = reinterpret_cast<uint8_t*>(data[i].as<intptr_t>());
      yuvBuffer->lineSize[i] = lineSize[i].as<int>();
    }
    return yuvBuffer;
  }

 private:
  explicit WebSoftwareDecoder(val decoder) : decoder(std::move(decoder)) {
  }

  val decoder;
};

std::unique_ptr<WebSoftwareDecoderFactory> WebSoftwareDecoderFactory::Make(val factory) {
  if (!factory.as<bool>()) {
    return nullptr;
  }
  return std::unique_ptr<WebSoftwareDecoderFactory>(
      new WebSoftwareDecoderFactory(std::move(factory)));
}

std::unique_ptr<SoftwareDecoder> WebSoftwareDecoderFactory::createSoftwareDecoder() {
  return WebSoftwareDecoder::Make(factory.call<val>("createSoftwareDecoder"));
}
}  // namespace pag
