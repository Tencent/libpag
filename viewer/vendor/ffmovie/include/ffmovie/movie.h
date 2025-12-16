///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2023 Tencent. All rights reserved.
//
//  This library is free software; you can redistribute it and/or modify it under the terms of the
//  GNU Lesser General Public License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
//  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
//  the GNU Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public License along with this
//  library; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
//  Boston, MA  02110-1301  USA
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <functional>
#include <unordered_map>
#include "pag/decoder.h"

#if defined(_WIN32)
#define FFMOVIE_API __declspec(dllexport)
#else
#define FFMOVIE_API __attribute__((visibility("default")))
#endif

namespace ffmovie {

class FFMOVIE_API SampleData {
 public:
  /**
   * Compression-encoded data after unpacking.
   */
  uint8_t* data = nullptr;
  /**
   * The size of each compression-encoded data.
   */
  size_t length = 0;

  SampleData() = default;

  SampleData(uint8_t* data, int64_t length) : data(data), length(length) {
  }

  bool empty() const {
    return data == nullptr || length == 0;
  }
};

struct FFMOVIE_API TimeRange {
  int64_t start;
  int64_t end;

  int64_t duration() const {
    return end - start;
  }

  bool isValid() const {
    return 0 <= start && start <= end;
  }

  bool contains(int64_t frame) const {
    return start <= frame && frame < end;
  }
};

/**
 * A container for data of bytes.
 */
class FFMOVIE_API ByteData {
 public:
  /**
   * Creates a ByteData object from the specified file path.
   */
  static std::unique_ptr<ByteData> FromPath(const std::string& filePath);
  /**
   * Creates a ByteData object and copy the specified data into it.
   */
  static std::unique_ptr<ByteData> MakeCopy(const void* data, size_t length);
  /**
   *  Call this when the data parameter is already const and will outlive the lifetime of the
   *  ByteData. Suitable for with const globals.
   */
  static std::unique_ptr<ByteData> MakeWithoutCopy(void* data, size_t length);
  /**
   * Creates a ByteData object and take ownership of the specified data. The specified data will be
   * deleted when the returned ByteData is released.
   * Set releaseCallback to release outer data, and default protocol is to call 'delete[] data',
   * if data is created by 'malloc', you need send a function to call 'free(data)'.
   */
  static std::unique_ptr<ByteData> MakeAdopted(
      uint8_t* data, size_t length, std::function<void(uint8_t*)> releaseCallback = DeleteCallback);
  /**
   * Creates a ByteData object with specified length.
   */
  static std::unique_ptr<ByteData> Make(size_t length);

  ~ByteData() {
    if (_releaseCallback) {
      _releaseCallback(_data);
    }
  }

  /**
   * Returns the memory address of byte data.
   */
  uint8_t* data() const {
    return _data;
  }

  /**
   * Returns the byte size.
   */
  size_t length() const {
    return _length;
  }

 private:
  static void DeleteCallback(uint8_t* data) {
    if (data) delete[] data;
  }

  ByteData(uint8_t* data, size_t length,
           std::function<void(uint8_t*)> releaseCallback = DeleteCallback)
      : _data(data), _length(length), _releaseCallback(std::move(releaseCallback)) {
  }

  uint8_t* _data;
  size_t _length;
  const std::function<void(uint8_t*)> _releaseCallback;
};

struct FFMOVIE_API AudioOutputConfig {
  // 采样率，默认 44.1kHZ
  int sampleRate = 44100;
  // 默认按每声道 1024 个采样点往外输出
  int outputSamplesCount = 1024;
  // 双声道
  int channels = 2;
};

enum class FFMOVIE_API NALUType {
  AnnexB,
  AVCC,
};

enum class FFMOVIE_API VideoProfile { BASELINE = 0, High };

struct FFMOVIE_API VideoExportConfig {

  int width = 1280;
  int height = 720;
  int frameRate = 30;
  int videoBitrate = 8000000;
  VideoProfile profile = VideoProfile::BASELINE;
};

struct FFMOVIE_API AudioExportConfig {
  int sampleRate = 44100;
  int channels = 2;
  int audioBitrate = 128000;
};

#define KEY_MIME "mime"
#define KEY_COLOR_SPACE "colorSpace"
#define KEY_COLOR_RANGE "colorRange"
#define KEY_WIDTH "width"
#define KEY_HEIGHT "height"
#define KEY_DURATION "durationUs"
#define KEY_FRAME_RATE "frame-rate"
#define KEY_ROTATION "rotation-degrees"
#define KEY_TRACK_ID "track-id"
#define MIMETYPE_UNKNOWN "unknown"
#define MIMETYPE_VIDEO_AVC "video/avc"
#define MIMETYPE_VIDEO_HEVC "video/hevc"
#define MIMETYPE_VIDEO_VP8 "video/vp8"
#define MIMETYPE_VIDEO_VP9 "video/vp9"
#define MIMETYPE_VIDEO_GIF "video/gif"
#define MIMETYPE_AUDIO_AAC "audio/aac"
#define MIMETYPE_AUDIO_MP3 "audio/mp3"
#define MIMETYPE_AUDIO_VORBIS "audio/vorbis"
#define MIMETYPE_AUDIO_OPUS "audio/opus"
#define MIMETYPE_AUDIO_PCM_S16LE "audio/pcm_s16le"
#define MIMETYPE_AUDIO_PCM_S16BE "audio/pcm_s16be"
#define MIMETYPE_AUDIO_PCM_F32LE "audio/pcm_f32le"
#define MIMETYPE_AUDIO_PCM_F32BE "audio/pcm_f32be"
#define MIMETYPE_AUDIO_PCM_MULAW "audio/pcm_mulaw"
#define MIMETYPE_AUDIO_PCM_U8 "audio/pcm_u8"
#define MIMETYPE_AUDIO_AC3 "audio/ac3"
#define MIMETYPE_AUDIO_AMR_NB "audio/amr_nb"
#define MIMETYPE_AUDIO_AMR_WB "audio/amr_wb"
#define COLORSPACE_REC601 "Rec601"
#define COLORSPACE_REC709 "Rec709"
#define COLORSPACE_REC2020 "Rec2020"
#define COLORRANGE_JPEG "RangeJPEG"
#define COLORRANGE_MPEG "RangeMPEG"
#define COLORRANGE_UNKNOWN "RangeUN"
#define KEY_VIDEO_MAX_REORDER "maxReorder"
#define KEY_SAMPLE_ASPECT_RATIO "sample_aspect_ratio"
#define KEY_TIME_BASE_NUM "time_base_num"
#define KEY_TIME_BASE_DEN "time_base_den"
#define KEY_AUDIO_FORMAT "audio_sample_format"
#define KEY_AUDIO_BITRATE "audio_bitrate"
#define KEY_VIDEO_BITRATE "video_bitrate"
#define KEY_AUDIO_SAMPLE_RATE "audio_sample_bitrate"
#define KEY_AUDIO_CHANNELS "audio_channels"
#define KEY_AUDIO_FRAME_SIZE "audio_frame_size"
#define KEY_TRACK_TYPE "track_type"
#define VIDEO_TRACK 1
#define AUDIO_TRACK 0

enum class FFMOVIE_API DecoderResult {
  Success = 0,

  TryAgainLater = -1,

  Error = -2,

  EndOfStream = -3
};

class FFMOVIE_API MediaFormat {
 public:
  MediaFormat() = default;
  ~MediaFormat();

  int getInteger(const std::string& name);
  int64_t getLong(const std::string& name);
  float getFloat(const std::string& name);
  std::string getString(const std::string& name);
  void* getCodecPar();
  void* getCodecContext() {
    return avCodecContext;
  }
  // Get video header information, AnnexB format
  std::vector<std::shared_ptr<ByteData>> headers();
  void setInteger(std::string name, int value);
  void setLong(std::string name, int64_t value);
  void setFloat(std::string name, float value);
  void setString(std::string name, std::string value);
  void setCodecPar(void* value);
  void setHeaders(std::vector<std::shared_ptr<ByteData>> headers);
  void setCodecContext(void* codecContext) {
    avCodecContext = codecContext;
  }

 private:
  void* avCodecContext = nullptr;
  std::unordered_map<std::string, std::string> trackFormatMap{};
  void* _codecPar = nullptr;
  std::vector<std::shared_ptr<ByteData>> _headers;
};

class FFMOVIE_API FFMediaDemuxer {
 public:
  static std::vector<std::string> SupportDemuxers();
  virtual ~FFMediaDemuxer() = default;
  virtual bool advance() = 0;
  virtual bool seekTo(int64_t timestamp) = 0;
  virtual int64_t getSampleTime() = 0;
  virtual int getTrackCount() = 0;
  virtual bool selectTrack(unsigned int index) = 0;
  virtual MediaFormat* getTrackFormat(unsigned int index) = 0;
  virtual SampleData readSampleData() const = 0;
  virtual int getCurrentTrackIndex() = 0;
};

class FFMOVIE_API FFAudioDemuxer : public FFMediaDemuxer {
 public:
  static std::unique_ptr<FFAudioDemuxer> Make(const std::string& path);
  static std::unique_ptr<FFAudioDemuxer> Make(uint8_t* data, size_t length);
};

class FFMOVIE_API FFVideoDemuxer : public FFMediaDemuxer {
 public:
  static std::unique_ptr<FFVideoDemuxer> Make(const std::string& path, NALUType startCodeType);

  virtual int64_t getSampleTimeAt(int64_t targetTime) = 0;

  virtual bool needSeeking(int64_t currentSampleTime, int64_t targetSampleTime) = 0;

  virtual void reset() = 0;
};

class FFMOVIE_API FFMediaDecoder {
 public:
  static std::vector<std::string> SupportDecoders();

  virtual ~FFMediaDecoder() = default;

  virtual DecoderResult onSendBytes(void* bytes, size_t length, int64_t timestamp) = 0;

  virtual DecoderResult onDecodeFrame() = 0;

  virtual DecoderResult onEndOfStream() = 0;

  virtual void onFlush() = 0;
};

class FFMOVIE_API FFAudioDecoder : public FFMediaDecoder {
 public:
  static std::unique_ptr<FFAudioDecoder> Make(FFMediaDemuxer* demuxer,
                                              std::shared_ptr<AudioOutputConfig> config);
  virtual SampleData onRenderFrame() = 0;
  virtual int64_t currentPresentationTime() = 0;
};

enum class FFMOVIE_API CodingResult {
  CodingConfig = 1,
  CodingSuccess = 0,
  TryAgainLater = -1,
  CodingError = -2,
  EndOfStream = -3,
};

#define NOPTS_VALUE ((int64_t)UINT64_C(0x8000000000000000))

struct FFMOVIE_API EncodePacket {
  std::unique_ptr<ByteData> data;
  bool isKeyFrame = false;
  int64_t pts = -1;
  int64_t dts = NOPTS_VALUE;
};

class FFMOVIE_API FFMediaMuxer {
 public:
  static std::shared_ptr<FFMediaMuxer> Make();
  virtual ~FFMediaMuxer() = default;
  virtual bool initMuxer(const std::string& filePath) = 0;
  virtual bool start() = 0;
  virtual bool stop() = 0;
  virtual int addTrack(std::shared_ptr<MediaFormat> mediaFormat) = 0;
  virtual bool writeFrame(int streamIndex, void* packet) = 0;
  virtual bool writeFrame(int streamIndex, std::shared_ptr<EncodePacket> packet) = 0;
  virtual void refreshExtraData(int streamIndex,
                                const std::vector<std::shared_ptr<ByteData>>& header) = 0;
  virtual void collectErrorMsgs(std::vector<std::string>* const toMsgs) = 0;
};

class FFMOVIE_API FFEncoder {
 public:
  virtual ~FFEncoder() = default;
  virtual bool initEncoder() = 0;
  virtual CodingResult onEndOfStream() = 0;
  virtual CodingResult onEncodeData(void** packet) = 0;
  virtual std::shared_ptr<MediaFormat> getMediaFormat() = 0;
  virtual void collectErrorMsgs(std::vector<std::string>* const toMsgs) = 0;
};

class FFMOVIE_API FFAudioEncoder : public FFEncoder {
 public:
  static std::unique_ptr<FFAudioEncoder> Make(const AudioExportConfig& config);
  virtual CodingResult onSendData(uint8_t* data, int64_t length, int sampleCount) = 0;
};

class FFMOVIE_API FFVideoEncoder : public FFEncoder {
 public:
  static std::unique_ptr<FFVideoEncoder> Make(const VideoExportConfig& config);
  virtual CodingResult onSendData(std::unique_ptr<ByteData> rgbaData, int width, int height,
                                  int rowBytes, int64_t pts) = 0;
};

}  // namespace ffmovie
