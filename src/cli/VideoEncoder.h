/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace pagx::cli {

/**
 * Describes one output container. Only mp4 ships today; additional formats also need their alpha
 * and muxer behavior wired into the command and export paths.
 */
struct VideoFormatSpec {
  /** The --format value, e.g. "mp4". */
  std::string name = {};
  /** The ffmpeg -f value. It is always passed explicitly because the ".part" file has no suffix
   *  ffmpeg could infer the container from. */
  std::string container = {};
  /** Encoder used when VideoEncodeOptions::encoder is empty. */
  std::string defaultEncoder = {};
  /** Pixel format used by both -pix_fmt and the scale filter. */
  std::string pixelFormat = {};
  /** Whether frames must be opaque. H.264 cannot carry alpha, so the mp4 entry sets this true. */
  bool requiresOpaque = true;
};

/** Returns the spec for a --format value, or nullptr when the format is not a video container. */
const VideoFormatSpec* FindVideoFormat(const std::string& format);

/**
 * Encoder settings. The dimensions must be the ones the frames are rendered at: the encoder
 * validates them against the pixel format instead of rescaling.
 */
struct VideoEncodeOptions {
  int width = 0;
  int height = 0;
  int frameRate = 0;
  /** Constant rate factor, 0-51 with lower meaning better quality. Negative omits -crf, for
   *  encoders that have no CRF option such as h264_videotoolbox. */
  int crf = 20;
  /** Encoder preset. Empty omits -preset, for encoders that have no preset option. */
  std::string preset = "medium";
  /** Encoder name; empty means the format's default encoder. */
  std::string encoder = {};
  /** Extra arguments placed after the generated codec/color options and before the fixed muxer
   *  and output arguments. */
  std::vector<std::string> extraArgs = {};
  /** Final output path. The encoder writes a unique sibling temporary file and publishes it on
   *  success. Command previews use "<outputFile>.part" as the temporary-path placeholder. */
  std::string outputFile = {};
};

/**
 * Builds the ffmpeg argv for the rawvideo stdin pipeline; arguments[0] is `ffmpegPath`. Returns
 * false and fills `errorMsg` when the options cannot form a valid command line.
 */
bool BuildFFmpegCommandLine(const std::string& ffmpegPath, const VideoFormatSpec& format,
                            const VideoEncodeOptions& options, std::vector<std::string>* arguments,
                            std::string* errorMsg);

/** Joins argv into a single shell-like line, for --dry-run output and error messages. */
std::string FormatCommandLine(const std::vector<std::string>& arguments);

/** Splits arguments on whitespace with single or double quotes. Reports unclosed quotes and
 *  preserves explicitly quoted empty arguments. */
bool SplitArguments(const std::string& text, std::vector<std::string>* arguments,
                    std::string* errorMsg = nullptr);

/** Result of looking for the ffmpeg executable. */
struct FFmpegLocation {
  std::string path = {};
  /** Source of `path`: "--ffmpeg", "FFMPEG_BIN" or "PATH". Empty when nothing was found. */
  std::string source = {};
  /** Why the lookup failed; empty when `path` is set. */
  std::string error = {};
};

/** Finds ffmpeg, in order: `explicitPath`, then the FFMPEG_BIN environment variable, then PATH. */
FFmpegLocation FindFFmpeg(const std::string& explicitPath);

/**
 * Streams premultiplied RGBA frames to one ffmpeg child process through its stdin, then atomically
 * publishes a unique sibling temporary file onto the output path once ffmpeg exits cleanly.
 *
 * ffmpeg keeps the inherited stderr, so its own diagnostics reach the terminal unchanged. This
 * class only reports why a step failed and what the child exit code was. It is a CLI-internal,
 * non-thread-safe component: concurrent Start() calls on different threads are not supported, and
 * process waits have no cancellation or timeout.
 */
class VideoEncoder {
 public:
  /**
   * Starts ffmpeg for `format`. Returns nullptr and fills `errorMsg` when the command line cannot
   * be built or the child process cannot be spawned.
   */
  static std::unique_ptr<VideoEncoder> Start(const std::string& ffmpegPath,
                                             const VideoFormatSpec& format,
                                             const VideoEncodeOptions& options,
                                             std::string* errorMsg = nullptr);

  ~VideoEncoder();

  VideoEncoder(const VideoEncoder&) = delete;
  VideoEncoder& operator=(const VideoEncoder&) = delete;

  /**
   * Writes one frame. `rowBytes` may be larger than width * 4, in which case the rows are packed
   * first: rawvideo has no stride concept, so an unpacked write would shear the picture.
   */
  bool writeFrame(const uint8_t* pixels, size_t rowBytes, std::string* errorMsg = nullptr);

  /** Closes the pipe, waits for ffmpeg, then publishes the temporary file onto the output path. */
  bool finish(std::string* errorMsg = nullptr);

  /** Requests ffmpeg termination, reaps it without a timeout, and deletes the owned temporary
   *  file. A no-op once the encoder has stopped. */
  void abort();

 private:
  struct ChildProcess;

  VideoEncoder(const VideoEncodeOptions& options, std::string partFile);

  bool spawnChild(const std::vector<std::string>& arguments, std::string* errorMsg);
  bool spawnChildWindows(const std::vector<std::string>& arguments, std::string* errorMsg);
  bool spawnChildPosix(const std::vector<std::string>& arguments, std::string* errorMsg);
  bool writeAll(const uint8_t* data, size_t size, std::string* errorMsg);
  void closeStdin();
  bool reapChild(int* exitCode);
  void killChild();
  void removePartFile();

  int width = 0;
  int height = 0;
  std::string outputFile = {};
  std::string partFile = {};
  std::vector<uint8_t> packedBuffer = {};
  std::unique_ptr<ChildProcess> child = {};
  int64_t framesWritten = 0;
  bool stopped = false;
};

}  // namespace pagx::cli
