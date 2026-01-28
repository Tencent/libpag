/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#ifdef PAG_USE_WIN_HARDWARE_DECODER

#include "HardwareDecoder.h"
#include <codecapi.h>
#include <d3d11_1.h>
#include <mfapi.h>
#include <mftransform.h>
#include "WinYUVData.h"
#include "base/utils/Log.h"
#include "tgfx/core/Buffer.h"
#include "tgfx/platform/win/D3D11TextureReader.h"

namespace pag {

// ============================================================================
// D3D11DecoderContext implementation
// ============================================================================

std::mutex D3D11DecoderContext::instanceMutex;
std::weak_ptr<D3D11DecoderContext> D3D11DecoderContext::instance;

std::shared_ptr<D3D11DecoderContext> D3D11DecoderContext::GetInstance() {
  std::lock_guard<std::mutex> lock(instanceMutex);
  auto ctx = instance.lock();
  if (!ctx) {
    ctx = std::shared_ptr<D3D11DecoderContext>(new D3D11DecoderContext());
    if (!ctx->isValid()) {
      return nullptr;
    }
    instance = ctx;
  }
  return ctx;
}

void D3D11DecoderContext::ReleaseInstance() {
  std::lock_guard<std::mutex> lock(instanceMutex);
  instance.reset();
}

D3D11DecoderContext::D3D11DecoderContext() {
  valid = init();
}

D3D11DecoderContext::~D3D11DecoderContext() {
  deviceManager.Reset();
  d3d11Context.Reset();
  d3d11Device.Reset();
}

bool D3D11DecoderContext::init() {
  // Create D3D11 device
  D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0,
                                       D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};

  D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
  UINT flags = D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

  HRESULT hr =
      D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, featureLevels,
                        _countof(featureLevels), D3D11_SDK_VERSION, d3d11Device.GetAddressOf(),
                        &featureLevel, d3d11Context.GetAddressOf());

  if (FAILED(hr)) {
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, &featureLevels[1],
                           _countof(featureLevels) - 1, D3D11_SDK_VERSION, d3d11Device.GetAddressOf(),
                           &featureLevel, d3d11Context.GetAddressOf());
  }

  if (FAILED(hr)) {
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags, nullptr, 0,
                           D3D11_SDK_VERSION, d3d11Device.GetAddressOf(), &featureLevel,
                           d3d11Context.GetAddressOf());
  }

  if (FAILED(hr)) {
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, flags, nullptr, 0,
                           D3D11_SDK_VERSION, d3d11Device.GetAddressOf(), &featureLevel,
                           d3d11Context.GetAddressOf());
  }

  if (FAILED(hr)) {
    LOGE("D3D11DecoderContext: Failed to create D3D11 device, hr=0x%08X", hr);
    return false;
  }

  // Enable multi-threading
  ComPtr<ID3D10Multithread> multithread;
  hr = d3d11Device.As(&multithread);
  if (SUCCEEDED(hr)) {
    multithread->SetMultithreadProtected(TRUE);
  }

  // Create DXGI Device Manager
  hr = MFCreateDXGIDeviceManager(&resetToken, deviceManager.GetAddressOf());
  if (FAILED(hr)) {
    LOGE("D3D11DecoderContext: Failed to create DXGI device manager.");
    return false;
  }

  hr = deviceManager->ResetDevice(d3d11Device.Get(), resetToken);
  if (FAILED(hr)) {
    LOGE("D3D11DecoderContext: Failed to reset DXGI device manager.");
    return false;
  }

  LOGI("D3D11DecoderContext: Initialized successfully.");
  return true;
}

// ============================================================================
// HardwareDecoder implementation
// ============================================================================

HardwareDecoder::HardwareDecoder(const VideoFormat& format) : videoFormat(format) {
  isValid = initDecoder(format);
}

HardwareDecoder::~HardwareDecoder() {
  mfTransform.Reset();
  decodedTexture.Reset();
  textureReader = nullptr;
  d3d11Context = nullptr;  // Release shared context reference
}

bool HardwareDecoder::initDecoder(const VideoFormat& format) {
  // Get shared D3D11 context
  d3d11Context = D3D11DecoderContext::GetInstance();
  if (!d3d11Context || !d3d11Context->isValid()) {
    LOGE("HardwareDecoder: Failed to get D3D11 context.");
    return false;
  }

  // Check if WGL_NV_DX_interop is available
  useTextureSharing = tgfx::IsNVDXInteropAvailable();

  if (!initMFTransform(format)) {
    return false;
  }

  // If texture sharing is available, create texture reader for D3D11-OpenGL interop
  if (useTextureSharing) {
    textureReader = tgfx::D3D11TextureReader::Make(format.width, format.height,
                                                    d3d11Context->getDevice(),
                                                    d3d11Context->getDeviceContext());
    if (!textureReader) {
      useTextureSharing = false;
    }
  }

  // For non-texture-sharing path, allocate CPU buffer for NV12 readback
  if (!useTextureSharing) {
    yPlaneSize = static_cast<size_t>(format.width) * static_cast<size_t>(format.height);
    uvPlaneSize = static_cast<size_t>(format.width) * static_cast<size_t>((format.height + 1) / 2);
    yuvBuffer.resize(yPlaneSize + uvPlaneSize);
  }

  LOGI("HardwareDecoder: Initialized successfully.");
  return true;
}

bool HardwareDecoder::initMFTransform(const VideoFormat& format) {
  // Find hardware decoder MFT
  GUID codecGUID = (format.mimeType == "video/hevc") ? MFVideoFormat_HEVC : MFVideoFormat_H264;
  LOGI("HardwareDecoder: Looking for %s decoder.", format.mimeType.c_str());

  MFT_REGISTER_TYPE_INFO inputType = {MFMediaType_Video, codecGUID};

  IMFActivate** activates = nullptr;
  UINT32 count = 0;

  HRESULT hr = MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT,
                         &inputType, nullptr, &activates, &count);

  if (FAILED(hr) || count == 0) {
    LOGE("HardwareDecoder: No decoder found for codec %s (hr=0x%08X, count=%u).",
         format.mimeType.c_str(), hr, count);
    return false;
  }

  bool foundDecoder = false;
  for (UINT32 i = 0; i < count && !foundDecoder; i++) {
    hr = activates[i]->ActivateObject(IID_PPV_ARGS(mfTransform.GetAddressOf()));
    if (FAILED(hr)) {
      continue;
    }

    // Check if decoder supports D3D11
    ComPtr<IMFAttributes> attributes;
    hr = mfTransform->GetAttributes(attributes.GetAddressOf());
    if (SUCCEEDED(hr)) {
      UINT32 d3d11Aware = 0;
      if (SUCCEEDED(attributes->GetUINT32(MF_SA_D3D11_AWARE, &d3d11Aware)) && d3d11Aware) {
        foundDecoder = true;

        // Check and unlock async MFT if needed
        UINT32 asyncFlag = 0;
        if (SUCCEEDED(attributes->GetUINT32(MF_TRANSFORM_ASYNC, &asyncFlag)) && asyncFlag) {
          attributes->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
          isAsyncMFT = true;

          hr = mfTransform.As(&eventGenerator);
          if (FAILED(hr)) {
            isAsyncMFT = false;
          }
        }
      } else {
        mfTransform.Reset();
      }
    } else {
      foundDecoder = true;
    }
  }

  for (UINT32 i = 0; i < count; i++) {
    activates[i]->Release();
  }
  CoTaskMemFree(activates);

  if (!foundDecoder || !mfTransform) {
    LOGE("HardwareDecoder: No D3D11-capable decoder found.");
    return false;
  }

  // Set D3D11 device manager from shared context
  hr = mfTransform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER,
                                   reinterpret_cast<ULONG_PTR>(d3d11Context->getDeviceManager()));
  if (FAILED(hr)) {
    LOGE("HardwareDecoder: Failed to set D3D11 device manager.");
    return false;
  }

  if (!configureInputType(format)) {
    LOGE("HardwareDecoder: Failed to configure input type.");
    return false;
  }

  if (!configureOutputType()) {
    LOGE("HardwareDecoder: Failed to configure output type.");
    return false;
  }

  hr = mfTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
  if (FAILED(hr)) {
    LOGE("HardwareDecoder: Failed to start streaming.");
    return false;
  }

  hr = mfTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
  if (FAILED(hr)) {
    LOGE("HardwareDecoder: Failed to notify start of stream.");
    return false;
  }

  return true;
}

bool HardwareDecoder::configureInputType(const VideoFormat& format) {
  ComPtr<IMFMediaType> inputType;
  HRESULT hr = MFCreateMediaType(inputType.GetAddressOf());
  if (FAILED(hr)) {
    return false;
  }

  GUID codecGUID = (format.mimeType == "video/hevc") ? MFVideoFormat_HEVC : MFVideoFormat_H264;

  hr = inputType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
  if (FAILED(hr)) {
    return false;
  }

  hr = inputType->SetGUID(MF_MT_SUBTYPE, codecGUID);
  if (FAILED(hr)) {
    return false;
  }

  hr = MFSetAttributeSize(inputType.Get(), MF_MT_FRAME_SIZE, static_cast<UINT32>(format.width),
                          static_cast<UINT32>(format.height));
  if (FAILED(hr)) {
    return false;
  }

  // Set codec private data (SPS/PPS for H.264, VPS/SPS/PPS for HEVC)
  if (!format.headers.empty()) {
    size_t totalSize = 0;
    for (const auto& header : format.headers) {
      totalSize += header->size();
    }

    codecHeaders.resize(totalSize);
    size_t pos = 0;
    for (const auto& header : format.headers) {
      memcpy(codecHeaders.data() + pos, header->data(), header->size());
      pos += header->size();
    }

    inputType->SetBlob(MF_MT_MPEG_SEQUENCE_HEADER, codecHeaders.data(),
                            static_cast<UINT32>(codecHeaders.size()));
  }

  hr = mfTransform->SetInputType(0, inputType.Get(), 0);
  return SUCCEEDED(hr);
}

bool HardwareDecoder::configureOutputType() {
  DWORD typeIndex = 0;
  ComPtr<IMFMediaType> outputType;

  while (SUCCEEDED(mfTransform->GetOutputAvailableType(0, typeIndex, outputType.GetAddressOf()))) {
    GUID subtype;
    if (SUCCEEDED(outputType->GetGUID(MF_MT_SUBTYPE, &subtype))) {
      if (IsEqualGUID(subtype, MFVideoFormat_NV12)) {
        HRESULT hr = mfTransform->SetOutputType(0, outputType.Get(), 0);
        return SUCCEEDED(hr);
      }
    }
    outputType.Reset();
    typeIndex++;
  }

  LOGE("HardwareDecoder: NV12 output format not supported.");
  return false;
}

DecodingResult HardwareDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  if (!isValid || !bytes || length == 0) {
    return DecodingResult::Error;
  }

  auto* inputData = static_cast<uint8_t*>(bytes);
  bool needPrependHeaders = false;
  
  if (!headersSent && !codecHeaders.empty() && length >= 5) {
    int nalOffset = 0;
    if (inputData[0] == 0 && inputData[1] == 0 && inputData[2] == 0 && inputData[3] == 1) {
      nalOffset = 4;
    } else if (inputData[0] == 0 && inputData[1] == 0 && inputData[2] == 1) {
      nalOffset = 3;
    }
    
    if (nalOffset > 0) {
      uint8_t nalType = inputData[nalOffset] & 0x1F;
      if (nalType == 5) {
        needPrependHeaders = true;
      }
    }
  }

  size_t totalLength = length;
  if (needPrependHeaders) {
    totalLength += codecHeaders.size();
  }

  ComPtr<IMFSample> sample;
  ComPtr<IMFMediaBuffer> buffer;

  HRESULT hr = MFCreateMemoryBuffer(static_cast<DWORD>(totalLength), buffer.GetAddressOf());
  if (FAILED(hr)) {
    LOGE("HardwareDecoder: Failed to create memory buffer.");
    return DecodingResult::Error;
  }

  BYTE* bufferData = nullptr;
  hr = buffer->Lock(&bufferData, nullptr, nullptr);
  if (FAILED(hr)) {
    return DecodingResult::Error;
  }

  size_t offset = 0;
  if (needPrependHeaders) {
    memcpy(bufferData, codecHeaders.data(), codecHeaders.size());
    offset = codecHeaders.size();
    headersSent = true;
  }
  memcpy(bufferData + offset, bytes, length);
  
  buffer->Unlock();
  buffer->SetCurrentLength(static_cast<DWORD>(totalLength));

  hr = MFCreateSample(sample.GetAddressOf());
  if (FAILED(hr)) {
    return DecodingResult::Error;
  }

  sample->AddBuffer(buffer.Get());
  sample->SetSampleTime(time * 10);
  sample->SetSampleDuration(0);

  hr = mfTransform->ProcessInput(0, sample.Get(), 0);
  if (hr == MF_E_NOTACCEPTING) {
    return DecodingResult::TryAgainLater;
  } else if (FAILED(hr)) {
    return DecodingResult::Error;
  }

  flushing = false;
  endOfStream = false;
  return DecodingResult::Success;
}

DecodingResult HardwareDecoder::onDecodeFrame() {
  if (!isValid) {
    return DecodingResult::Error;
  }

  HRESULT hr;

  if (isAsyncMFT && eventGenerator) {
    ComPtr<IMFMediaEvent> event;
    hr = eventGenerator->GetEvent(MF_EVENT_FLAG_NO_WAIT, event.GetAddressOf());
    
    if (hr == MF_E_NO_EVENTS_AVAILABLE) {
      if (endOfStream) {
        return DecodingResult::EndOfStream;
      }
      return DecodingResult::TryAgainLater;
    } else if (FAILED(hr)) {
      if (endOfStream) {
        return DecodingResult::EndOfStream;
      }
      return DecodingResult::TryAgainLater;
    }

    MediaEventType eventType;
    hr = event->GetType(&eventType);
    if (FAILED(hr)) {
      return DecodingResult::Error;
    }

    if (eventType == METransformNeedInput) {
      return DecodingResult::TryAgainLater;
    } else if (eventType == METransformHaveOutput) {
      // MFT has output ready, fall through to ProcessOutput
    } else if (eventType == METransformDrainComplete) {
      return DecodingResult::EndOfStream;
    } else {
      return DecodingResult::TryAgainLater;
    }
  }

  MFT_OUTPUT_STREAM_INFO streamInfo;
  hr = mfTransform->GetOutputStreamInfo(0, &streamInfo);
  if (FAILED(hr)) {
    return DecodingResult::Error;
  }

  // Check if MFT provides its own samples
  bool providesSamples = (streamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES) != 0;

  ComPtr<IMFSample> outputSample;
  ComPtr<IMFMediaBuffer> outputBuffer;

  if (!providesSamples) {
    // We need to provide the output sample
    hr = MFCreateSample(outputSample.GetAddressOf());
    if (FAILED(hr)) {
      return DecodingResult::Error;
    }

    DWORD bufferSize = streamInfo.cbSize > 0 ? streamInfo.cbSize : 1920 * 1080 * 2;
    hr = MFCreateMemoryBuffer(bufferSize, outputBuffer.GetAddressOf());
    if (FAILED(hr)) {
      return DecodingResult::Error;
    }

    outputSample->AddBuffer(outputBuffer.Get());
  }

  MFT_OUTPUT_DATA_BUFFER outputData = {};
  outputData.dwStreamID = 0;
  outputData.pSample = providesSamples ? nullptr : outputSample.Get();

  DWORD status = 0;
  hr = mfTransform->ProcessOutput(0, 1, &outputData, &status);

  if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT) {
    if (outputData.pEvents) {
      outputData.pEvents->Release();
    }
    if (endOfStream) {
      return DecodingResult::EndOfStream;
    }
    return DecodingResult::TryAgainLater;
  } else if (hr == MF_E_TRANSFORM_STREAM_CHANGE) {
    // Output format changed, reconfigure
    if (outputData.pEvents) {
      outputData.pEvents->Release();
    }
    configureOutputType();
    return DecodingResult::TryAgainLater;
  } else if (FAILED(hr)) {
    if (outputData.pEvents) {
      outputData.pEvents->Release();
    }
    return DecodingResult::Error;
  }

  // Get the actual output sample
  ComPtr<IMFSample> resultSample =
      providesSamples ? outputData.pSample : outputSample;

  if (outputData.pEvents) {
    outputData.pEvents->Release();
  }

  if (!resultSample) {
    return DecodingResult::TryAgainLater;
  }

  // Get sample time
  LONGLONG sampleTime = 0;
  if (SUCCEEDED(resultSample->GetSampleTime(&sampleTime))) {
    currentPTS = sampleTime / 10;  // Convert from 100ns to microseconds
  }

  // Get the D3D11 texture from the output sample
  ComPtr<IMFMediaBuffer> mediaBuffer;
  hr = resultSample->GetBufferByIndex(0, mediaBuffer.GetAddressOf());
  if (FAILED(hr)) {
    if (providesSamples && outputData.pSample) {
      outputData.pSample->Release();
    }
    return DecodingResult::Error;
  }

  ComPtr<IMFDXGIBuffer> dxgiBuffer;
  hr = mediaBuffer.As(&dxgiBuffer);
  if (SUCCEEDED(hr)) {
    // Hardware decoded frame - get D3D11 texture
    ComPtr<ID3D11Texture2D> texture;
    hr = dxgiBuffer->GetResource(IID_PPV_ARGS(texture.GetAddressOf()));
    if (SUCCEEDED(hr)) {
      UINT subresourceIndex = 0;
      dxgiBuffer->GetSubresourceIndex(&subresourceIndex);

      decodedTexture = texture;
      decodedSubresourceIndex = static_cast<int>(subresourceIndex);
      hasDecodedFrame = true;

      if (useTextureSharing) {
        if (textureReader && !textureReader->updateTexture(texture.Get(), static_cast<int>(subresourceIndex))) {
          LOGE("HardwareDecoder: Failed to update shared texture in onDecodeFrame.");
        }
      } else {
        if (!copyTextureToBuffer(texture.Get(), static_cast<int>(subresourceIndex))) {
          LOGE("HardwareDecoder: Failed to copy texture to buffer in onDecodeFrame.");
        }
      }
    }
  } else {
    LOGE("HardwareDecoder: Got non-DXGI buffer from hardware decoder.");
    if (providesSamples && outputData.pSample) {
      outputData.pSample->Release();
    }
    return DecodingResult::Error;
  }

  if (providesSamples && outputData.pSample) {
    outputData.pSample->Release();
  }

  return DecodingResult::Success;
}

void HardwareDecoder::onFlush() {
  if (!isValid) {
    return;
  }

  mfTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
  hasDecodedFrame = false;
  decodedTexture.Reset();
  currentPTS = -1;
  flushing = true;
  endOfStream = false;
}

DecodingResult HardwareDecoder::onEndOfStream() {
  if (!isValid) {
    return DecodingResult::Error;
  }

  HRESULT hr = mfTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0);
  if (FAILED(hr)) {
    LOGE("HardwareDecoder: Failed to send end of stream.");
    return DecodingResult::Error;
  }

  hr = mfTransform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0);
  if (FAILED(hr)) {
    LOGE("HardwareDecoder: Failed to drain decoder.");
    return DecodingResult::Error;
  }

  flushing = true;
  endOfStream = true;
  return DecodingResult::Success;
}

int64_t HardwareDecoder::presentationTime() {
  return currentPTS;
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::onRenderFrame() {
  if (!isValid || !hasDecodedFrame) {
    return nullptr;
  }

  if (useTextureSharing) {
    return renderFrameNVIDIA();
  } else {
    return renderFrameYUV();
  }
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::renderFrameNVIDIA() {
  if (!textureReader || !hasDecodedFrame) {
    return nullptr;
  }

  // Texture was already updated in onDecodeFrame, just acquire the ImageBuffer
  return textureReader->acquireNextBuffer();
}

std::shared_ptr<tgfx::ImageBuffer> HardwareDecoder::renderFrameYUV() {
  if (!hasDecodedFrame) {
    LOGE("HardwareDecoder: No decoded frame available.");
    return nullptr;
  }

  // Data was already copied to yuvBuffer in onDecodeFrame
  // Create YUV data from NV12 buffer
  auto yuvData =
      WinYUVData::MakeNV12(videoFormat.width, videoFormat.height, yuvBuffer.data(),
                           videoFormat.width, yuvBuffer.data() + yPlaneSize, videoFormat.width);

  if (!yuvData) {
    LOGE("HardwareDecoder: Failed to create YUV data.");
    return nullptr;
  }

  return tgfx::ImageBuffer::MakeNV12(std::move(yuvData), videoFormat.colorSpace);
}

bool HardwareDecoder::copyTextureToBuffer(ID3D11Texture2D* texture, int subresourceIndex) {
  if (!texture || !d3d11Context) {
    return false;
  }

  auto device = d3d11Context->getDevice();
  auto context = d3d11Context->getDeviceContext();

  // Get source texture description
  D3D11_TEXTURE2D_DESC srcDesc;
  texture->GetDesc(&srcDesc);

  // Always create a new staging texture to avoid cache issues
  ComPtr<ID3D11Texture2D> localStagingTexture;
  D3D11_TEXTURE2D_DESC stagingDesc = {};
  stagingDesc.Width = srcDesc.Width;
  stagingDesc.Height = srcDesc.Height;
  stagingDesc.MipLevels = 1;
  stagingDesc.ArraySize = 1;
  stagingDesc.Format = DXGI_FORMAT_NV12;
  stagingDesc.SampleDesc.Count = 1;
  stagingDesc.Usage = D3D11_USAGE_STAGING;
  stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

  HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, localStagingTexture.GetAddressOf());
  if (FAILED(hr)) {
    LOGE("HardwareDecoder: Failed to create staging texture, hr=0x%08X", hr);
    return false;
  }

  // Copy from GPU texture to staging texture
  if (srcDesc.ArraySize > 1) {
    // Texture array - copy specific subresource
    UINT srcSubresource = D3D11CalcSubresource(0, static_cast<UINT>(subresourceIndex), 1);
    context->CopySubresourceRegion(localStagingTexture.Get(), 0, 0, 0, 0, texture,
                                   srcSubresource, nullptr);
  } else {
    context->CopyResource(localStagingTexture.Get(), texture);
  }

  // Use a query to wait for GPU to complete the copy operation
  D3D11_QUERY_DESC queryDesc = {};
  queryDesc.Query = D3D11_QUERY_EVENT;
  ComPtr<ID3D11Query> query;
  hr = device->CreateQuery(&queryDesc, query.GetAddressOf());
  if (SUCCEEDED(hr)) {
    context->End(query.Get());
    context->Flush();
    BOOL queryData = FALSE;
    while (context->GetData(query.Get(), &queryData, sizeof(queryData), 0) == S_FALSE) {
      // Spin wait for GPU to complete
    }
  } else {
    context->Flush();
  }

  // Map staging texture
  D3D11_MAPPED_SUBRESOURCE mappedY;
  hr = context->Map(localStagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedY);
  if (FAILED(hr)) {
    LOGE("HardwareDecoder: Failed to map Y plane, hr=0x%08X", hr);
    return false;
  }

  // Copy Y plane to yuvBuffer
  uint8_t* src = static_cast<uint8_t*>(mappedY.pData);
  uint8_t* dst = yuvBuffer.data();
  for (int y = 0; y < videoFormat.height; y++) {
    memcpy(dst + y * videoFormat.width, src + y * mappedY.RowPitch,
           static_cast<size_t>(videoFormat.width));
  }

  // Copy UV plane
  // UV plane is continuous in the same subresource (after Y plane)
  // Use staging texture height (srcDesc.Height) for UV offset
  src = static_cast<uint8_t*>(mappedY.pData) + mappedY.RowPitch * srcDesc.Height;
  dst = yuvBuffer.data() + yPlaneSize;
  int uvHeight = (videoFormat.height + 1) / 2;
  for (int y = 0; y < uvHeight; y++) {
    memcpy(dst + y * videoFormat.width, src + y * mappedY.RowPitch,
           static_cast<size_t>(videoFormat.width));
  }

  context->Unmap(localStagingTexture.Get(), 0);

  return true;
}

}  // namespace pag

#endif  // PAG_USE_WIN_HARDWARE_DECODER
