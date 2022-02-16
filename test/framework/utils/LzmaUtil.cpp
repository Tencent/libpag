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

#include "LzmaUtil.h"
#include "test/framework/lzma/Lzma2DecMt.h"
#include "test/framework/lzma/Lzma2Enc.h"

namespace pag {
static void* LzmaAlloc(ISzAllocPtr, size_t size) {
  return new uint8_t[size];
}

static void LzmaFree(ISzAllocPtr, void* address) {
  if (!address) {
    return;
  }
  delete[] reinterpret_cast<uint8_t*>(address);
}

static ISzAlloc gAllocFuncs = {LzmaAlloc, LzmaFree};

class SequentialOutStream {
 public:
  virtual ~SequentialOutStream() = default;

  virtual bool write(const void* data, size_t size) = 0;
};

class SequentialInStream {
 public:
  virtual ~SequentialInStream() = default;

  virtual bool read(void* data, size_t size, size_t* processedSize) = 0;
};

struct CSeqInStreamWrap {
  ISeqInStream vt;
  std::unique_ptr<SequentialInStream> inStream;
};

struct CSeqOutStreamWrap {
  ISeqOutStream vt;
  std::unique_ptr<SequentialOutStream> outStream;
};

class BuffPtrInStream : public SequentialInStream {
 public:
  explicit BuffPtrInStream(const uint8_t* buffer, size_t bufferSize)
      : buffer(buffer), bufferSize(bufferSize) {
  }

  bool read(void* data, size_t size, size_t* processedSize) override {
    if (processedSize) {
      *processedSize = 0;
    }
    if (size == 0 || position >= bufferSize) {
      return true;
    }
    auto remain = bufferSize - position;
    if (remain > size) {
      remain = size;
    }
    memcpy(data, static_cast<const uint8_t*>(buffer) + position, remain);
    position += remain;
    if (processedSize) {
      *processedSize = remain;
    }
    return true;
  }

 private:
  const uint8_t* buffer = nullptr;
  size_t bufferSize = 0;
  size_t position = 0;
};

class VectorOutStream : public SequentialOutStream {
 public:
  explicit VectorOutStream(std::vector<uint8_t>* buffer) : buffer(buffer) {
  }

  bool write(const void* data, size_t size) override {
    auto oldSize = buffer->size();
    buffer->resize(oldSize + size);
    memcpy(&(*buffer)[oldSize], data, size);
    return true;
  }

 private:
  std::vector<uint8_t>* buffer;
};

class BuffPtrSeqOutStream : public SequentialOutStream {
 public:
  BuffPtrSeqOutStream(uint8_t* buffer, size_t size) : buffer(buffer), bufferSize(size) {
  }

  bool write(const void* data, size_t size) override {
    auto remain = bufferSize - position;
    if (remain > size) {
      remain = size;
    }
    if (remain != 0) {
      memcpy(buffer + position, data, remain);
      position += remain;
    }
    return remain != 0 || size == 0;
  }

 private:
  uint8_t* buffer = nullptr;
  size_t bufferSize = 0;
  size_t position = 0;
};

static const size_t kStreamStepSize = 1 << 31;

static SRes MyRead(const ISeqInStream* p, void* data, size_t* size) {
  CSeqInStreamWrap* wrap = CONTAINER_FROM_VTBL(p, CSeqInStreamWrap, vt);
  auto curSize = (*size < kStreamStepSize) ? *size : kStreamStepSize;
  if (!wrap->inStream->read(data, curSize, &curSize)) {
    return SZ_ERROR_READ;
  }
  *size = curSize;
  return SZ_OK;
}

static size_t MyWrite(const ISeqOutStream* p, const void* buf, size_t size) {
  auto* wrap = CONTAINER_FROM_VTBL(p, CSeqOutStreamWrap, vt);
  if (wrap->outStream->write(buf, size)) {
    return size;
  }
  return 0;
}

class Lzma2Encoder {
 public:
  Lzma2Encoder() {
    encoder = Lzma2Enc_Create(&gAllocFuncs, &gAllocFuncs);
  }

  ~Lzma2Encoder() {
    Lzma2Enc_Destroy(encoder);
  }

  std::shared_ptr<tgfx::Data> code(const std::shared_ptr<tgfx::Data>& inputData) {
    if (encoder == nullptr || inputData == nullptr || inputData->size() == 0) {
      return nullptr;
    }
    auto inputSize = inputData->size();
    CLzma2EncProps lzma2Props;
    Lzma2EncProps_Init(&lzma2Props);
    lzma2Props.lzmaProps.dictSize = inputSize;
    lzma2Props.lzmaProps.level = 9;
    lzma2Props.numTotalThreads = 4;
    Lzma2Enc_SetProps(encoder, &lzma2Props);
    std::vector<uint8_t> outBuf;
    outBuf.resize(1 + 8);
    outBuf[0] = Lzma2Enc_WriteProperties(encoder);
    for (int i = 0; i < 8; i++) {
      outBuf[1 + i] = static_cast<uint8_t>(inputSize >> (8 * i));
    }
    CSeqInStreamWrap inWrap = {};
    inWrap.vt.Read = MyRead;
    inWrap.inStream = std::make_unique<BuffPtrInStream>(
        static_cast<const uint8_t*>(inputData->data()), inputSize);
    CSeqOutStreamWrap outStream = {};
    outStream.vt.Write = MyWrite;
    outStream.outStream = std::make_unique<VectorOutStream>(&outBuf);
    auto status =
        Lzma2Enc_Encode2(encoder, &outStream.vt, nullptr, nullptr, &inWrap.vt, nullptr, 0, nullptr);
    if (status != SZ_OK) {
      return nullptr;
    }
    return tgfx::Data::MakeWithCopy(&outBuf[0], outBuf.size());
  }

 private:
  CLzma2EncHandle encoder = nullptr;
};

std::shared_ptr<tgfx::Data> LzmaUtil::Compress(const std::shared_ptr<tgfx::Data>& pixelData) {
  Lzma2Encoder encoder;
  return encoder.code(pixelData);
}

class Lzma2Decoder {
 public:
  Lzma2Decoder() {
    decoder = Lzma2DecMt_Create(&gAllocFuncs, &gAllocFuncs);
  }

  ~Lzma2Decoder() {
    if (decoder) {
      Lzma2DecMt_Destroy(decoder);
    }
  }

  std::shared_ptr<tgfx::Data> code(const std::shared_ptr<tgfx::Data>& inputData) {
    if (decoder == nullptr || inputData == nullptr || inputData->size() == 0) {
      return nullptr;
    }
    auto input = static_cast<const uint8_t*>(inputData->data());
    auto inputSize = inputData->size() - 9;
    Byte prop = static_cast<const Byte*>(input)[0];

    CLzma2DecMtProps props;
    Lzma2DecMtProps_Init(&props);
    props.inBufSize_ST = inputSize;
    props.numThreads = 1;

    UInt64 outBufferSize = 0;
    for (int i = 0; i < 8; i++) {
      outBufferSize |= (input[1 + i] << (i * 8));
    }

    auto outBuffer = new uint8_t[outBufferSize];
    CSeqInStreamWrap inWrap = {};
    inWrap.vt.Read = MyRead;
    inWrap.inStream = std::make_unique<BuffPtrInStream>(input + 9, inputSize);
    CSeqOutStreamWrap outWrap = {};
    outWrap.vt.Write = MyWrite;
    outWrap.outStream = std::make_unique<BuffPtrSeqOutStream>(outBuffer, outBufferSize);
    UInt64 inProcessed = 0;
    int isMT = false;
    auto res = Lzma2DecMt_Decode(decoder, prop, &props, &outWrap.vt, &outBufferSize, 1, &inWrap.vt,
                                 &inProcessed, &isMT, nullptr);
    if (res == SZ_OK && inputSize == inProcessed) {
      return tgfx::Data::MakeAdopted(outBuffer, outBufferSize, tgfx::Data::DeleteProc);
    }
    delete[] outBuffer;
    return nullptr;
  }

 private:
  CLzma2DecMtHandle decoder = nullptr;
};

std::shared_ptr<tgfx::Data> LzmaUtil::Decompress(const std::shared_ptr<tgfx::Data>& data) {
  Lzma2Decoder decoder;
  return decoder.code(data);
}
}  // namespace pag
