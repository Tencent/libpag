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

#include "cli/VideoEncoder.h"
#ifdef _WIN32
#include <windows.h>
#else
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#endif
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <system_error>
#include <utility>
#include "cli/CliUtils.h"

namespace pagx::cli {

namespace {

constexpr int kMaxCrf = 51;

const char* const kPartSuffix = ".part";

const VideoFormatSpec kVideoFormats[] = {
    {"mp4", "mp4", "libx264", "yuv420p", true},
};

void SetError(std::string* errorMsg, const std::string& text) {
  if (errorMsg != nullptr) {
    *errorMsg = text;
  }
}

// 4:2:0 subsamples chroma by two in both directions, so an odd width or height cannot be encoded
// by those pixel formats at all.
bool NeedsEvenDimensions(const std::string& pixelFormat) {
  return pixelFormat == "yuv420p" || pixelFormat == "yuva420p" || pixelFormat == "yuv420p10le";
}

bool IsExecutableFile(const std::string& path) {
#ifdef _WIN32
  std::error_code ec;
  return std::filesystem::is_regular_file(path, ec) && !ec;
#else
  return access(path.c_str(), X_OK) == 0;
#endif
}

std::vector<std::string> FFmpegNames() {
#ifdef _WIN32
  return {"ffmpeg.exe", "ffmpeg"};
#else
  return {"ffmpeg"};
#endif
}

bool NeedsQuoting(const std::string& value) {
  if (value.empty()) {
    return true;
  }
  for (char ch : value) {
    if (std::isspace(static_cast<unsigned char>(ch)) != 0 || ch == '"' || ch == '\'' ||
        ch == '\\' || ch == '$' || ch == '`' || ch == '&' || ch == '|' || ch == ';' || ch == '<' ||
        ch == '>' || ch == '(' || ch == ')' || ch == '*' || ch == '?') {
      return true;
    }
  }
  return false;
}

std::string FrameWriteFailure(int64_t framesWritten, int exitCode) {
  std::string message =
      "ffmpeg stopped reading frames after " + std::to_string(framesWritten) + " frame(s)";
  if (exitCode != 0) {
    message += " (exit code " + std::to_string(exitCode) + ")";
  }
  return message;
}

std::string ExitCodeMessage(const std::string& action, int exitCode) {
  if (exitCode < 0) {
    return action + " (terminated by signal " + std::to_string(-exitCode) + ")";
  }
  return action + " (exit code " + std::to_string(exitCode) + ")";
}

}  // namespace

const VideoFormatSpec* FindVideoFormat(const std::string& format) {
  for (const auto& spec : kVideoFormats) {
    if (spec.name == format) {
      return &spec;
    }
  }
  return nullptr;
}

bool BuildFFmpegCommandLine(const std::string& ffmpegPath, const VideoFormatSpec& format,
                            const VideoEncodeOptions& options, std::vector<std::string>* arguments,
                            std::string* errorMsg) {
  if (arguments == nullptr) {
    SetError(errorMsg, "the argument list is null");
    return false;
  }
  if (ffmpegPath.empty()) {
    SetError(errorMsg, "the ffmpeg path is empty");
    return false;
  }
  if (options.width <= 0 || options.height <= 0) {
    SetError(errorMsg, "the output size must be positive");
    return false;
  }
  if (NeedsEvenDimensions(format.pixelFormat) &&
      ((options.width % 2) != 0 || (options.height % 2) != 0)) {
    SetError(errorMsg, "the " + format.pixelFormat + " pixel format requires an even width and " +
                           "height, got " + std::to_string(options.width) + "x" +
                           std::to_string(options.height));
    return false;
  }
  if (options.frameRate <= 0) {
    SetError(errorMsg, "the frame rate must be positive");
    return false;
  }
  if (options.crf > kMaxCrf) {
    SetError(errorMsg, "the crf must be within 0-" + std::to_string(kMaxCrf));
    return false;
  }
  if (options.outputFile.empty()) {
    SetError(errorMsg, "the output path is empty");
    return false;
  }

  auto& args = *arguments;
  args.clear();
  args.emplace_back(ffmpegPath);
  args.insert(args.end(), {"-hide_banner", "-loglevel", "error", "-y"});
  args.insert(args.end(), {"-f", "rawvideo", "-pixel_format", "rgba"});
  args.insert(args.end(), {"-video_size",
                           std::to_string(options.width) + "x" + std::to_string(options.height)});
  args.insert(args.end(), {"-framerate", std::to_string(options.frameRate), "-i", "pipe:0"});
  args.emplace_back("-an");
  args.insert(args.end(),
              {"-c:v", options.encoder.empty() ? format.defaultEncoder : options.encoder});
  if (!options.preset.empty()) {
    args.insert(args.end(), {"-preset", options.preset});
  }
  if (options.crf >= 0) {
    args.insert(args.end(), {"-crf", std::to_string(options.crf)});
  }
  args.insert(args.end(), {"-pix_fmt", format.pixelFormat});
  // The swscale conversion matrix is decided by the scale filter, not by the -colorspace metadata
  // options below, so both have to be spelled out to keep players from shifting the colors.
  args.insert(args.end(),
              {"-vf", "scale=in_color_matrix=bt709:out_color_matrix=bt709:out_range=tv,format=" +
                          format.pixelFormat});
  args.insert(args.end(), {"-colorspace", "bt709", "-color_primaries", "bt709", "-color_trc",
                           "bt709", "-color_range", "tv"});
  args.insert(args.end(), options.extraArgs.begin(), options.extraArgs.end());
  args.insert(args.end(), {"-movflags", "+faststart", "-f", format.container,
                           options.outputFile + kPartSuffix});
  return true;
}

std::string FormatCommandLine(const std::vector<std::string>& arguments) {
  std::string line = {};
  for (const auto& argument : arguments) {
    if (!line.empty()) {
      line += ' ';
    }
    if (!NeedsQuoting(argument)) {
      line += argument;
      continue;
    }
    line += '"';
    for (char ch : argument) {
      if (ch == '"' || ch == '\\' || ch == '$' || ch == '`') {
        line += '\\';
      }
      line += ch;
    }
    line += '"';
  }
  return line;
}

std::vector<std::string> SplitArguments(const std::string& text) {
  std::vector<std::string> arguments = {};
  std::string current = {};
  bool inQuotes = false;
  for (char ch : text) {
    if (ch == '"') {
      inQuotes = !inQuotes;
      continue;
    }
    if (!inQuotes && std::isspace(static_cast<unsigned char>(ch)) != 0) {
      if (!current.empty()) {
        arguments.emplace_back(std::move(current));
        current.clear();
      }
      continue;
    }
    current += ch;
  }
  if (!current.empty()) {
    arguments.emplace_back(std::move(current));
  }
  return arguments;
}

FFmpegLocation FindFFmpeg(const std::string& explicitPath) {
  FFmpegLocation location = {};
  if (!explicitPath.empty()) {
    if (!IsExecutableFile(explicitPath)) {
      location.error = "the --ffmpeg path '" + explicitPath + "' is not an executable file";
      return location;
    }
    location.path = explicitPath;
    location.source = "--ffmpeg";
    return location;
  }
  const char* env = std::getenv("FFMPEG_BIN");
  if (env != nullptr && env[0] != '\0') {
    if (!IsExecutableFile(env)) {
      location.error =
          "FFMPEG_BIN is set to '" + std::string(env) + "', which is not an executable file";
      return location;
    }
    location.path = env;
    location.source = "FFMPEG_BIN";
    return location;
  }
  const char* pathEnv = std::getenv("PATH");
#ifdef _WIN32
  constexpr char kListSeparator = ';';
#else
  constexpr char kListSeparator = ':';
#endif
  auto names = FFmpegNames();
  if (pathEnv != nullptr) {
    std::string pathList = pathEnv;
    size_t start = 0;
    while (start <= pathList.size()) {
      auto end = pathList.find(kListSeparator, start);
      if (end == std::string::npos) {
        end = pathList.size();
      }
      auto directory = pathList.substr(start, end - start);
      start = end + 1;
      // An empty entry means the current directory, which would let an ffmpeg binary planted next
      // to the input file win over the one the user installed.
      if (directory.empty()) {
        continue;
      }
      for (const auto& name : names) {
        auto candidate = directory + "/" + name;
        if (IsExecutableFile(candidate)) {
          location.path = candidate;
          location.source = "PATH";
          return location;
        }
      }
    }
  }
  location.error = "ffmpeg was not found in PATH";
  return location;
}

struct VideoEncoder::ChildProcess {
#ifdef _WIN32
  HANDLE process = nullptr;
  HANDLE thread = nullptr;
  HANDLE stdinPipe = nullptr;
#else
  pid_t processId = -1;
  int stdinPipe = -1;
#endif
  bool reaped = false;
  int exitCode = 0;
};

VideoEncoder::VideoEncoder(const VideoFormatSpec& formatSpec, const VideoEncodeOptions& options)
    : format(formatSpec), width(options.width), height(options.height),
      frameRate(options.frameRate), outputFile(options.outputFile),
      partFile(options.outputFile + kPartSuffix) {
}

VideoEncoder::~VideoEncoder() {
  abort();
}

std::unique_ptr<VideoEncoder> VideoEncoder::Start(const std::string& ffmpegPath,
                                                  const VideoFormatSpec& format,
                                                  const VideoEncodeOptions& options,
                                                  std::string* errorMsg) {
  std::vector<std::string> arguments = {};
  if (!BuildFFmpegCommandLine(ffmpegPath, format, options, &arguments, errorMsg)) {
    return nullptr;
  }
  std::unique_ptr<VideoEncoder> encoder(new VideoEncoder(format, options));
  if (!encoder->spawnChild(arguments, errorMsg)) {
    return nullptr;
  }
  return encoder;
}

bool VideoEncoder::spawnChild(const std::vector<std::string>& arguments, std::string* errorMsg) {
  auto process = std::make_unique<ChildProcess>();
#ifdef _WIN32
  std::wstring commandLine = {};
  for (const auto& argument : arguments) {
    auto wide = UTF8ToWide(argument);
    if (wide.empty() && !argument.empty()) {
      SetError(errorMsg, "failed to encode the ffmpeg command line as UTF-16");
      return false;
    }
    if (!commandLine.empty()) {
      commandLine += L' ';
    }
    commandLine += QuoteWindowsArg(wide);
  }
  SECURITY_ATTRIBUTES security = {};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  HANDLE readPipe = nullptr;
  HANDLE writePipe = nullptr;
  if (!CreatePipe(&readPipe, &writePipe, &security, 0)) {
    SetError(errorMsg, "failed to create the ffmpeg input pipe (Windows error " +
                           std::to_string(GetLastError()) + ")");
    return false;
  }
  if (!SetHandleInformation(writePipe, HANDLE_FLAG_INHERIT, 0)) {
    SetError(errorMsg, "failed to configure the ffmpeg input pipe (Windows error " +
                           std::to_string(GetLastError()) + ")");
    CloseHandle(readPipe);
    CloseHandle(writePipe);
    return false;
  }
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = readPipe;
  startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
  PROCESS_INFORMATION info = {};
  if (!CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
                      &startup, &info)) {
    SetError(errorMsg,
             "failed to spawn ffmpeg (Windows error " + std::to_string(GetLastError()) + ")");
    CloseHandle(readPipe);
    CloseHandle(writePipe);
    return false;
  }
  CloseHandle(readPipe);
  process->process = info.hProcess;
  process->thread = info.hThread;
  process->stdinPipe = writePipe;
  child = std::move(process);
  return true;
#else
  int fds[2] = {-1, -1};
  if (pipe(fds) != 0) {
    SetError(errorMsg,
             "failed to create the ffmpeg input pipe: " + std::string(std::strerror(errno)));
    return false;
  }
  std::vector<char*> argv = {};
  argv.reserve(arguments.size() + 1);
  for (const auto& argument : arguments) {
    argv.emplace_back(const_cast<char*>(argument.c_str()));
  }
  argv.emplace_back(nullptr);
  // ffmpeg may exit before the last frame is written; without this the resulting SIGPIPE would
  // kill the CLI before it can report the child exit code.
  ::signal(SIGPIPE, SIG_IGN);
  auto processId = fork();
  if (processId < 0) {
    SetError(errorMsg, "failed to fork the ffmpeg process: " + std::string(std::strerror(errno)));
    close(fds[0]);
    close(fds[1]);
    return false;
  }
  if (processId == 0) {
    if (dup2(fds[0], STDIN_FILENO) >= 0) {
      close(fds[0]);
      close(fds[1]);
      execvp(argv[0], argv.data());
    }
    _exit(127);
  }
  close(fds[0]);
  process->processId = processId;
  process->stdinPipe = fds[1];
  child = std::move(process);
  return true;
#endif
}

bool VideoEncoder::writeFrame(const uint8_t* pixels, size_t rowBytes, std::string* errorMsg) {
  if (stopped || child == nullptr) {
    SetError(errorMsg, "the encoder is not running");
    return false;
  }
  if (pixels == nullptr) {
    SetError(errorMsg, "the frame buffer is null");
    return false;
  }
  auto packedRowBytes = static_cast<size_t>(width) * 4;
  if (rowBytes < packedRowBytes) {
    SetError(errorMsg, "the row stride " + std::to_string(rowBytes) +
                           " is smaller than width * 4 (" + std::to_string(packedRowBytes) + ")");
    return false;
  }
  const uint8_t* data = pixels;
  size_t size = rowBytes * static_cast<size_t>(height);
  if (rowBytes != packedRowBytes) {
    packedBuffer.resize(packedRowBytes * static_cast<size_t>(height));
    for (int row = 0; row < height; ++row) {
      std::memcpy(packedBuffer.data() + static_cast<size_t>(row) * packedRowBytes,
                  pixels + static_cast<size_t>(row) * rowBytes, packedRowBytes);
    }
    data = packedBuffer.data();
    size = packedBuffer.size();
  }
  if (!writeAll(data, size, errorMsg)) {
    return false;
  }
  framesWritten++;
  return true;
}

bool VideoEncoder::writeAll(const uint8_t* data, size_t size, std::string* errorMsg) {
#ifdef _WIN32
  size_t offset = 0;
  while (offset < size) {
    auto chunk = static_cast<DWORD>(std::min<size_t>(size - offset, size_t{1} << 20));
    DWORD written = 0;
    if (WriteFile(child->stdinPipe, data + offset, chunk, &written, nullptr)) {
      offset += written;
      continue;
    }
    auto error = GetLastError();
    closeStdin();
    auto exitCode = reapChild();
    stopped = true;
    removePartFile();
    if (error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA) {
      SetError(errorMsg, FrameWriteFailure(framesWritten, exitCode));
    } else {
      SetError(errorMsg,
               "failed to write a frame to ffmpeg (Windows error " + std::to_string(error) + ")");
    }
    return false;
  }
  return true;
#else
  size_t offset = 0;
  while (offset < size) {
    auto written = write(child->stdinPipe, data + offset, size - offset);
    if (written > 0) {
      offset += static_cast<size_t>(written);
      continue;
    }
    if (written < 0 && errno == EINTR) {
      continue;
    }
    auto error = errno;
    closeStdin();
    auto exitCode = reapChild();
    stopped = true;
    removePartFile();
    if (error == EPIPE) {
      SetError(errorMsg, FrameWriteFailure(framesWritten, exitCode));
    } else {
      SetError(errorMsg, "failed to write a frame to ffmpeg: " + std::string(std::strerror(error)));
    }
    return false;
  }
  return true;
#endif
}

void VideoEncoder::closeStdin() {
  if (child == nullptr) {
    return;
  }
#ifdef _WIN32
  if (child->stdinPipe != nullptr) {
    CloseHandle(child->stdinPipe);
    child->stdinPipe = nullptr;
  }
#else
  if (child->stdinPipe >= 0) {
    close(child->stdinPipe);
    child->stdinPipe = -1;
  }
#endif
}

int VideoEncoder::reapChild() {
  if (child == nullptr) {
    return 0;
  }
  if (child->reaped) {
    return child->exitCode;
  }
#ifdef _WIN32
  WaitForSingleObject(child->process, INFINITE);
  DWORD code = 0;
  GetExitCodeProcess(child->process, &code);
  child->exitCode = static_cast<int>(code);
  CloseHandle(child->process);
  child->process = nullptr;
  if (child->thread != nullptr) {
    CloseHandle(child->thread);
    child->thread = nullptr;
  }
#else
  int status = 0;
  while (waitpid(child->processId, &status, 0) < 0) {
    if (errno != EINTR) {
      break;
    }
  }
  if (WIFEXITED(status)) {
    child->exitCode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    child->exitCode = -WTERMSIG(status);
  } else {
    child->exitCode = 1;
  }
#endif
  child->reaped = true;
  return child->exitCode;
}

void VideoEncoder::killChild() {
  if (child == nullptr || child->reaped) {
    return;
  }
#ifdef _WIN32
  if (child->process != nullptr) {
    TerminateProcess(child->process, 1);
  }
#else
  ::kill(child->processId, SIGKILL);
#endif
  reapChild();
}

void VideoEncoder::removePartFile() {
  std::error_code ec;
  std::filesystem::remove(partFile, ec);
}

bool VideoEncoder::finish(std::string* errorMsg) {
  if (stopped || child == nullptr) {
    SetError(errorMsg, "the encoder is not running");
    return false;
  }
  closeStdin();
  auto exitCode = reapChild();
  stopped = true;
  if (exitCode != 0) {
    removePartFile();
    SetError(errorMsg, ExitCodeMessage(
                           "ffmpeg failed to encode " + std::to_string(framesWritten) + " frame(s)",
                           exitCode));
    return false;
  }
  std::error_code removeError = {};
  std::filesystem::remove(outputFile, removeError);
  std::error_code renameError = {};
  std::filesystem::rename(partFile, outputFile, renameError);
  if (renameError) {
    removePartFile();
    SetError(errorMsg, "failed to move '" + partFile + "' to '" + outputFile + "'");
    return false;
  }
  return true;
}

void VideoEncoder::abort() {
  if (stopped) {
    return;
  }
  stopped = true;
  closeStdin();
  killChild();
  removePartFile();
}

}  // namespace pagx::cli
