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

#include "SoftAVCDecoder.h"
#include <cstdlib>
#include "tgfx/core/Buffer.h"

#ifdef PAG_USE_LIBAVC

#ifdef _WIN32
#include <malloc.h>
#endif

#if __APPLE__
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define IOS 1
#endif
#endif

namespace pag {
#ifdef _WIN32
static void* ivd_aligned_malloc(void*, WORD32 alignment, WORD32 size) {
  return _aligned_malloc(size, alignment);
}

static void ivd_aligned_free(void*, void* block) {
  _aligned_free(block);
}
#elif IOS
static void* ivd_aligned_malloc(void*, WORD32, WORD32 size) {
  return malloc(size);
}

static void ivd_aligned_free(void*, void* block) {
  free(block);
}
#else

static void* ivd_aligned_malloc(void*, WORD32 alignment, WORD32 size) {
  void* buffer = nullptr;
  if (posix_memalign(&buffer, alignment, size) != 0) {
    return nullptr;
  }
  return buffer;
}

static void ivd_aligned_free(void*, void* block) {
  free(block);
}

#endif

bool SoftAVCDecoder::onConfigure(const std::vector<HeaderData>& headers, std::string mimeType, int,
                                 int) {
  if (mimeType != "video/avc") {
    return false;
  }
  if (!initDecoder()) {
    return false;
  }
  size_t totalLength = 0;
  for (auto& header : headers) {
    totalLength += header.length;
  }
  tgfx::Buffer buffer(totalLength);
  size_t pos = 0;
  for (auto& header : headers) {
    buffer.writeRange(pos, header.length, header.data);
    pos += header.length;
  }
  headerData = buffer.release();
  return openDecoder();
}

SoftAVCDecoder::~SoftAVCDecoder() {
  destroyDecoder();
  delete outputFrame;
}

DecoderResult SoftAVCDecoder::onSendBytes(void* bytes, size_t length, int64_t time) {
  decodeInput.e_cmd = IVD_CMD_VIDEO_DECODE;
  decodeInput.u4_ts = static_cast<UWORD32>(time);
  decodeInput.pv_stream_buffer = bytes;
  decodeInput.u4_num_Bytes = static_cast<UWORD32>(length);
  decodeInput.u4_size = sizeof(ih264d_video_decode_ip_t);
  return DecoderResult::Success;
}

DecoderResult SoftAVCDecoder::onDecodeFrame() {
  flushed = false;
  decodeOutput.u4_size = sizeof(ih264d_video_decode_op_t);
  auto result = ih264d_api_function(codecContext, &decodeInput, &decodeOutput);
  if (result != IV_SUCCESS) {
    LOGE("SoftAVCDecoder: Error on sending bytes for decoding, time:%lld \n", time);
    return DecoderResult::Error;
  }
  return decodeOutput.u4_output_present ? DecoderResult::Success : DecoderResult::TryAgainLater;
}

DecoderResult SoftAVCDecoder::onEndOfStream() {
  ivd_ctl_flush_ip_t s_ctl_ip = {};
  ivd_ctl_flush_op_t s_ctl_op = {};
  s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
  s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_FLUSH;
  s_ctl_ip.u4_size = sizeof(ivd_ctl_flush_ip_t);
  s_ctl_op.u4_size = sizeof(ivd_ctl_flush_op_t);
  auto result = ih264d_api_function(codecContext, &s_ctl_ip, &s_ctl_op);
  if (result != IV_SUCCESS) {
    return DecoderResult::Error;
  }
  return onSendBytes(nullptr, 0, 0);
}

void SoftAVCDecoder::onFlush() {
  if (flushed) {
    return;
  }
  resetDecoder();
  openDecoder();
  flushed = true;
}

std::unique_ptr<YUVBuffer> SoftAVCDecoder::onRenderFrame() {
  if (decodeOutput.u4_output_present == 0) {
    return nullptr;
  }
  auto output = std::make_unique<YUVBuffer>();
  auto& buffer = decodeInput.s_out_buffer;
  memcpy(output->data, buffer.pu1_bufs, sizeof(output->data));
  auto format = decodeOutput.s_disp_frm_buf;
  output->lineSize[0] = static_cast<int>(format.u4_y_strd);
  output->lineSize[1] = static_cast<int>(format.u4_u_strd);
  output->lineSize[2] = static_cast<int>(format.u4_v_strd);
  return output;
}

bool SoftAVCDecoder::initDecoder() {
  IV_API_CALL_STATUS_T status;
  codecContext = nullptr;
  ih264d_create_ip_t s_create_ip;
  ih264d_create_op_t s_create_op;
  s_create_ip.s_ivd_create_ip_t.u4_size = sizeof(ih264d_create_ip_t);
  s_create_ip.s_ivd_create_ip_t.e_cmd = IVD_CMD_CREATE;
  s_create_ip.s_ivd_create_ip_t.u4_share_disp_buf = 0;
  s_create_op.s_ivd_create_op_t.u4_size = sizeof(ih264d_create_op_t);
  s_create_ip.s_ivd_create_ip_t.e_output_format = IV_YUV_420P;
  s_create_ip.s_ivd_create_ip_t.pf_aligned_alloc = ivd_aligned_malloc;
  s_create_ip.s_ivd_create_ip_t.pf_aligned_free = ivd_aligned_free;
  s_create_ip.s_ivd_create_ip_t.pv_mem_ctxt = nullptr;
  status = ih264d_api_function(codecContext, &s_create_ip, &s_create_op);
  if (status != IV_SUCCESS) {
    LOGE("SoftAVCDecoder: Error on initializing a decoder, error code: 0x%x",
         s_create_op.s_ivd_create_op_t.u4_error_code);
    destroyDecoder();
    codecContext = nullptr;
    return false;
  }
  codecContext = reinterpret_cast<iv_obj_t*>(s_create_op.s_ivd_create_op_t.pv_handle);
  codecContext->pv_fxns = reinterpret_cast<void*>(ih264d_api_function);
  codecContext->u4_size = sizeof(iv_obj_t);
  return true;
}

bool SoftAVCDecoder::openDecoder() {
  if (!setNumCores()) {
    return false;
  }
  if (!setParams(true)) {
    return false;
  }
  onSendBytes(const_cast<void*>(headerData->data()), headerData->size(), 0);
  auto result = onDecodeFrame();
  if (result == DecoderResult::Error) {
    return false;
  }
  if (outputFrame == nullptr) {
    if (!initOutputFrame()) {
      return false;
    }
  }
  flushed = true;
  return setParams(false);
}

bool SoftAVCDecoder::initOutputFrame() {
  ivd_ctl_getbufinfo_ip_t s_ctl_ip;
  ivd_ctl_getbufinfo_op_t s_ctl_op;
  s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
  s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_GETBUFINFO;
  s_ctl_ip.u4_size = sizeof(ivd_ctl_getbufinfo_ip_t);
  s_ctl_op.u4_size = sizeof(ivd_ctl_getbufinfo_op_t);
  auto status = ih264d_api_function(codecContext, &s_ctl_ip, &s_ctl_op);
  if (status != IV_SUCCESS) {
    return false;
  }
  uint32_t outLength = 0;
  for (uint32_t i = 0; i < s_ctl_op.u4_min_num_out_bufs; i++) {
    outLength += s_ctl_op.u4_min_out_buf_size[i];
  }
  outputFrame = new tgfx::Buffer(outLength);
  auto& ps_out_buf = decodeInput.s_out_buffer;
  size_t offset = 0;
  for (uint32_t i = 0; i < s_ctl_op.u4_min_num_out_bufs; i++) {
    ps_out_buf.u4_min_out_buf_size[i] = s_ctl_op.u4_min_out_buf_size[i];
    ps_out_buf.pu1_bufs[i] = outputFrame->bytes() + offset;
    offset += s_ctl_op.u4_min_out_buf_size[i];
  }
  ps_out_buf.u4_num_bufs = s_ctl_op.u4_min_num_out_bufs;
  return true;
}

void SoftAVCDecoder::resetDecoder() {
  ivd_ctl_reset_ip_t s_ctl_ip;
  ivd_ctl_reset_op_t s_ctl_op;
  IV_API_CALL_STATUS_T status;
  s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
  s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_RESET;
  s_ctl_ip.u4_size = sizeof(ivd_ctl_reset_ip_t);
  s_ctl_op.u4_size = sizeof(ivd_ctl_reset_op_t);
  status = ih264d_api_function(codecContext, &s_ctl_ip, &s_ctl_op);
  if (IV_SUCCESS != status) {
    LOGE("SoftAVCDecoder: Error on resetting a decoder, error code:: 0x%x", s_ctl_op.u4_error_code);
    return;
  }
}

void SoftAVCDecoder::destroyDecoder() {
  IV_API_CALL_STATUS_T status;
  if (codecContext) {
    ih264d_delete_ip_t s_delete_ip;
    ih264d_delete_op_t s_delete_op;
    s_delete_ip.s_ivd_delete_ip_t.u4_size = sizeof(ih264d_delete_ip_t);
    s_delete_ip.s_ivd_delete_ip_t.e_cmd = IVD_CMD_DELETE;
    s_delete_op.s_ivd_delete_op_t.u4_size = sizeof(ih264d_delete_op_t);
    status = ih264d_api_function(codecContext, &s_delete_ip, &s_delete_op);
    if (status != IV_SUCCESS) {
      LOGE("SoftAVCDecoder: Error on destroying a decoder, error code: 0x%x",
           s_delete_op.s_ivd_delete_op_t.u4_error_code);
    }
  }
}

bool SoftAVCDecoder::setParams(bool decodeHeader) {
  ivd_ctl_set_config_ip_t s_ctl_ip;
  ivd_ctl_set_config_op_t s_ctl_op;
  s_ctl_ip.u4_disp_wd = 0;
  s_ctl_ip.e_frm_skip_mode = IVD_SKIP_NONE;
  s_ctl_ip.e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
  s_ctl_ip.e_vid_dec_mode = decodeHeader ? IVD_DECODE_HEADER : IVD_DECODE_FRAME;
  s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
  s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
  s_ctl_ip.u4_size = sizeof(ivd_ctl_set_config_ip_t);
  s_ctl_op.u4_size = sizeof(ivd_ctl_set_config_op_t);
  auto status = ih264d_api_function(codecContext, &s_ctl_ip, &s_ctl_op);
  if (status != IV_SUCCESS) {
    LOGE("SoftAVCDecoder: Error on setting the stride: 0x%x", s_ctl_op.u4_error_code);
  }
  return status == IV_SUCCESS;
}

bool SoftAVCDecoder::setNumCores() {
  ih264d_ctl_set_num_cores_ip_t s_set_cores_ip;
  ih264d_ctl_set_num_cores_op_t s_set_cores_op;
  s_set_cores_ip.e_cmd = IVD_CMD_VIDEO_CTL;
  s_set_cores_ip.e_sub_cmd = (IVD_CONTROL_API_COMMAND_TYPE_T)IH264D_CMD_CTL_SET_NUM_CORES;
  s_set_cores_ip.u4_num_cores = 1;
  s_set_cores_ip.u4_size = sizeof(ih264d_ctl_set_num_cores_ip_t);
  s_set_cores_op.u4_size = sizeof(ih264d_ctl_set_num_cores_op_t);
  auto status = ih264d_api_function(codecContext, &s_set_cores_ip, &s_set_cores_op);
  if (IV_SUCCESS != status) {
    LOGE("SoftAVCDecoder: Error on setting number of cores: 0x%x", s_set_cores_op.u4_error_code);
  }
  return status == IV_SUCCESS;
}
}  // namespace pag

#endif
