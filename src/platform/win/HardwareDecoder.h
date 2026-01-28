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

#pragma once

#ifdef PAG_USE_WIN_HARDWARE_DECODER

#include <d3d11.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <memory>
#include <mutex>
#include <queue>
#include <wrl/client.h>
#include "rendering/video/VideoDecoder.h"
#include "rendering/video/VideoFormat.h"

using Microsoft::WRL::ComPtr;

namespace tgfx {
class D3D11TextureReader;
}

namespace pag {

/**
 * Shared D3D11 context for all HardwareDecoder instances.
 * This avoids creating multiple D3D11 devices and DXGI device managers.
 */
class D3D11DecoderContext {
 public:
  static std::shared_ptr<D3D11DecoderContext> GetInstance();
  static void ReleaseInstance();

  ~D3D11DecoderContext();

  ID3D11Device* getDevice() const { return d3d11Device.Get(); }
  ID3D11DeviceContext* getDeviceContext() const { return d3d11Context.Get(); }
  IMFDXGIDeviceManager* getDeviceManager() const { return deviceManager.Get(); }
  bool isValid() const { return valid; }

 private:
  D3D11DecoderContext();
  bool init();

  ComPtr<ID3D11Device> d3d11Device;
  ComPtr<ID3D11DeviceContext> d3d11Context;
  ComPtr<IMFDXGIDeviceManager> deviceManager;
  UINT resetToken = 0;
  bool valid = false;

  static std::mutex instanceMutex;
  static std::weak_ptr<D3D11DecoderContext> instance;
};

/**
 * Hardware video decoder for Windows platform using Media Foundation Transform (MFT) with D3D11.
 *
 * For NVIDIA GPUs:
 * - Uses MFT hardware decoder with D3D11
 * - Outputs D3D11 textures
 * - Shares textures with OpenGL via WGL_NV_DX_interop
 * - Creates TextureView for TGFX rendering (similar to Android SurfaceTexture)
 *
 * For AMD/Intel GPUs:
 * - Uses MFT hardware decoder with D3D11
 * - Reads back NV12 data from GPU
 * - Creates YUV ImageBuffer for TGFX rendering (similar to software decode)
 */
class HardwareDecoder : public VideoDecoder {
 public:
  ~HardwareDecoder() override;

  DecodingResult onSendBytes(void* bytes, size_t length, int64_t time) override;
  DecodingResult onEndOfStream() override;
  DecodingResult onDecodeFrame() override;
  void onFlush() override;
  int64_t presentationTime() override;
  std::shared_ptr<tgfx::ImageBuffer> onRenderFrame() override;

  bool isValid = false;

 private:
  explicit HardwareDecoder(const VideoFormat& format);

  bool initDecoder(const VideoFormat& format);
  bool initMFTransform(const VideoFormat& format);
  bool configureInputType(const VideoFormat& format);
  bool configureOutputType();

  // For NVIDIA: use D3D11-OpenGL interop
  std::shared_ptr<tgfx::ImageBuffer> renderFrameNVIDIA();

  // For AMD/Intel: read back NV12 data and create YUV ImageBuffer
  std::shared_ptr<tgfx::ImageBuffer> renderFrameYUV();

  // Copy NV12 texture data to CPU buffer
  bool copyTextureToBuffer(ID3D11Texture2D* texture, int subresourceIndex);

  // Shared D3D11 context
  std::shared_ptr<D3D11DecoderContext> d3d11Context;

  VideoFormat videoFormat;
  bool useTextureSharing = false;  // Whether to use D3D11-OpenGL texture sharing
  bool isAsyncMFT = false;  // Whether the MFT is asynchronous
  bool headersSent = false;  // Whether SPS/PPS headers have been sent to decoder

  // Cached codec headers (SPS/PPS) for prepending to first IDR frame
  std::vector<uint8_t> codecHeaders;

  // Media Foundation resources (per-decoder instance)
  ComPtr<IMFTransform> mfTransform;
  ComPtr<IMFMediaEventGenerator> eventGenerator;  // For async MFT

  // For NVIDIA: D3D11-OpenGL texture reader
  std::shared_ptr<tgfx::D3D11TextureReader> textureReader;

  // Decoded frame storage
  ComPtr<ID3D11Texture2D> decodedTexture;
  int decodedSubresourceIndex = 0;
  bool hasDecodedFrame = false;

  // CPU buffer for NV12 readback (AMD/Intel)
  std::vector<uint8_t> yuvBuffer;
  size_t yPlaneSize = 0;
  size_t uvPlaneSize = 0;

  // Timing
  int64_t currentPTS = -1;
  bool flushing = false;
  bool endOfStream = false;

  friend class HardwareDecoderFactory;
};

}  // namespace pag

#endif  // PAG_USE_WIN_HARDWARE_DECODER
