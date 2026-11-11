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
// ACL headers require the Win32 base types and macros from windows.h.
#include <accctrl.h>
#include <aclapi.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#endif
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <random>
#include <system_error>
#include <thread>
#include <utility>
#include "cli/CliUtils.h"

namespace pagx::cli {

namespace {

const char* const kPartSuffix = ".part";

const VideoFormatSpec kVideoFormats[] = {
    {"mp4", "mp4", "libx264", "yuv420p", true, true},
};

void SetError(std::string* errorMsg, const std::string& text) {
  if (errorMsg != nullptr) {
    *errorMsg = text;
  }
}

bool IsExecutableFile(const std::string& path) {
  std::error_code ec;
  if (!std::filesystem::is_regular_file(path, ec) || ec) {
    return false;
  }
#ifdef _WIN32
  return true;
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

bool ValidateEncoderArguments(const std::vector<std::string>& arguments, std::string* errorMsg) {
  static const char* const allowedOptions[] = {"-b:v", "-maxrate", "-bufsize", "-g",
                                               "-x264-params"};
  for (size_t index = 0; index < arguments.size(); ++index) {
    const auto& argument = arguments[index];
    bool allowed = false;
    for (const auto* option : allowedOptions) {
      if (argument == option) {
        allowed = true;
        break;
      }
    }
    if (!allowed) {
      SetError(errorMsg, "unsupported --encoder-args option '" + argument +
                             "'; only -b:v, -maxrate, -bufsize, -g, and -x264-params are allowed");
      return false;
    }
    if (index + 1 >= arguments.size()) {
      SetError(errorMsg, "missing value for --encoder-args option '" + argument + "'");
      return false;
    }
    ++index;
  }
  return true;
}

std::string FrameWriteFailure(int64_t framesWritten, int exitCode) {
  std::string message =
      "ffmpeg stopped reading frames after " + std::to_string(framesWritten) + " frame(s)";
  if (exitCode != 0) {
    message += " (exit code " + std::to_string(exitCode) + ")";
  }
  return message;
}

#ifndef _WIN32
std::string ExecErrorSuffix(int error) {
  if (error == 0) {
    return {};
  }
  return "; failed to execute ffmpeg: " + std::string(std::strerror(error));
}
#endif

std::string ExitCodeMessage(const std::string& action, int exitCode) {
  if (exitCode < 0) {
    return action + " (terminated by signal " + std::to_string(-exitCode) + ")";
  }
  return action + " (exit code " + std::to_string(exitCode) + ")";
}

std::error_code RemoveFileSystemPath(const std::string& path) {
  std::error_code error = {};
#ifdef _WIN32
  auto widePath = UTF8ToWide(path);
  if (widePath.empty()) {
    return std::make_error_code(std::errc::invalid_argument);
  }
  std::filesystem::remove(std::filesystem::path(widePath), error);
#else
  std::filesystem::remove(path, error);
#endif
  return error;
}

#ifdef _WIN32
bool CreatePrivateDirectory(const std::string& path, DWORD* error) {
  *error = ERROR_SUCCESS;
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    *error = GetLastError();
    return false;
  }
  DWORD tokenSize = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &tokenSize);
  if (tokenSize == 0) {
    *error = GetLastError();
    CloseHandle(token);
    return false;
  }
  std::vector<uint8_t> tokenData(tokenSize);
  if (!GetTokenInformation(token, TokenUser, tokenData.data(), tokenSize, &tokenSize)) {
    *error = GetLastError();
    CloseHandle(token);
    return false;
  }
  auto* tokenUser = reinterpret_cast<TOKEN_USER*>(tokenData.data());
  EXPLICIT_ACCESS_W access = {};
  access.grfAccessPermissions = GENERIC_ALL;
  access.grfAccessMode = SET_ACCESS;
  access.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  BuildTrusteeWithSidW(&access.Trustee, static_cast<PSID>(tokenUser->User.Sid));
  ACL* acl = nullptr;
  auto aclResult = SetEntriesInAclW(1, &access, nullptr, &acl);
  if (aclResult != ERROR_SUCCESS) {
    *error = aclResult;
    CloseHandle(token);
    return false;
  }
  SECURITY_DESCRIPTOR descriptor = {};
  SECURITY_ATTRIBUTES security = {};
  security.nLength = sizeof(security);
  security.lpSecurityDescriptor = &descriptor;
  if (!InitializeSecurityDescriptor(&descriptor, SECURITY_DESCRIPTOR_REVISION) ||
      !SetSecurityDescriptorDacl(&descriptor, TRUE, acl, FALSE)) {
    *error = GetLastError();
    LocalFree(acl);
    CloseHandle(token);
    return false;
  }
  auto widePath = UTF8ToWide(path);
  if (widePath.empty()) {
    *error = ERROR_INVALID_DATA;
    LocalFree(acl);
    CloseHandle(token);
    return false;
  }
  auto created = CreateDirectoryW(widePath.c_str(), &security) != 0;
  if (!created) {
    *error = GetLastError();
  }
  LocalFree(acl);
  CloseHandle(token);
  return created;
}
#else
bool CreatePrivateDirectory(const std::string& path, int* error) {
  if (mkdir(path.c_str(), 0700) == 0) {
    return true;
  }
  *error = errno;
  return false;
}
#endif

bool CreateStagingFile(const std::string& outputFile, std::string* partFile,
                       std::string* stagingDirectory, std::string* errorMsg) {
  std::random_device random = {};
  auto fileName = outputFile.substr(FindLastPathSeparator(outputFile) + 1);
  for (int attempt = 0; attempt < 100; ++attempt) {
    auto randomValue = (static_cast<uint64_t>(random()) << 32) ^ static_cast<uint64_t>(random());
    auto directory = outputFile + kPartSuffix + "." + std::to_string(randomValue) + ".staging";
    auto candidate = directory + "/" + fileName + kPartSuffix;
#ifdef _WIN32
    DWORD directoryError = ERROR_SUCCESS;
    if (!CreatePrivateDirectory(directory, &directoryError)) {
      if (directoryError == ERROR_FILE_EXISTS || directoryError == ERROR_ALREADY_EXISTS) {
        continue;
      }
      SetError(errorMsg, "failed to create a private staging directory (Windows error " +
                             std::to_string(directoryError) + ")");
      return false;
    }
    auto widePath = UTF8ToWide(candidate);
    if (widePath.empty()) {
      RemoveFileSystemPath(directory);
      SetError(errorMsg, "failed to encode the temporary output path as UTF-16");
      return false;
    }
    auto file = CreateFileW(widePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
      CloseHandle(file);
      *partFile = std::move(candidate);
      *stagingDirectory = std::move(directory);
      return true;
    }
    auto error = GetLastError();
    RemoveFileSystemPath(directory);
    SetError(errorMsg, "failed to create a temporary output file (Windows error " +
                           std::to_string(error) + ")");
    return false;
#else
    int directoryError = 0;
    if (!CreatePrivateDirectory(directory, &directoryError)) {
      if (directoryError == EEXIST) {
        continue;
      }
      SetError(errorMsg, "failed to create a private staging directory: " +
                             std::string(std::strerror(directoryError)));
      return false;
    }
    auto fd = open(candidate.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (fd >= 0) {
      if (close(fd) != 0) {
        auto error = errno;
        RemoveFileSystemPath(candidate);
        RemoveFileSystemPath(directory);
        SetError(errorMsg,
                 "failed to close the temporary output file: " + std::string(std::strerror(error)));
        return false;
      }
      *partFile = std::move(candidate);
      *stagingDirectory = std::move(directory);
      return true;
    }
    auto error = errno;
    RemoveFileSystemPath(directory);
    SetError(errorMsg,
             "failed to create a temporary output file: " + std::string(std::strerror(error)));
    return false;
#endif
  }
  SetError(errorMsg, "failed to allocate a unique temporary output file");
  return false;
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
  if (format.requiresEvenDimensions && ((options.width % 2) != 0 || (options.height % 2) != 0)) {
    SetError(errorMsg, "the " + format.pixelFormat + " pixel format requires an even width and " +
                           "height, got " + std::to_string(options.width) + "x" +
                           std::to_string(options.height));
    return false;
  }
  if (options.frameRate.numerator <= 0 || options.frameRate.denominator <= 0) {
    SetError(errorMsg, "the frame rate must be positive");
    return false;
  }
  auto frameRate = options.frameRate.normalized();
  if (frameRate.numerator > std::numeric_limits<int>::max() ||
      frameRate.denominator > std::numeric_limits<int>::max()) {
    SetError(errorMsg, "the frame rate is not representable by ffmpeg");
    return false;
  }
  if (options.crf > VideoEncoder::MAX_CRF) {
    SetError(errorMsg, "the crf must be within 0-" + std::to_string(VideoEncoder::MAX_CRF));
    return false;
  }
  if (options.outputFile.empty()) {
    SetError(errorMsg, "the output path is empty");
    return false;
  }
  if (!ValidateEncoderArguments(options.extraArgs, errorMsg)) {
    return false;
  }

  auto& args = *arguments;
  args.clear();
  args.emplace_back(ffmpegPath);
  args.insert(args.end(), {"-hide_banner", "-loglevel", "error", "-y"});
  args.insert(args.end(), {"-f", "rawvideo", "-pixel_format", "rgba"});
  args.insert(args.end(), {"-video_size",
                           std::to_string(options.width) + "x" + std::to_string(options.height)});
  args.insert(args.end(), {"-framerate", frameRate.toString(), "-i", "pipe:0"});
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

bool SplitArguments(const std::string& text, std::vector<std::string>* arguments,
                    std::string* errorMsg) {
  if (arguments == nullptr) {
    SetError(errorMsg, "the argument list is null");
    return false;
  }
  arguments->clear();
  std::string current = {};
  char quote = 0;
  bool hasToken = false;
  for (char ch : text) {
    if (quote != 0) {
      if (ch == quote) {
        quote = 0;
      } else {
        current += ch;
      }
      hasToken = true;
      continue;
    }
    if (ch == '"' || ch == '\'') {
      quote = ch;
      hasToken = true;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
      if (hasToken) {
        arguments->emplace_back(std::move(current));
        current.clear();
        hasToken = false;
      }
      continue;
    }
    current += ch;
    hasToken = true;
  }
  if (quote != 0) {
    arguments->clear();
    SetError(errorMsg, "unclosed quote in encoder arguments");
    return false;
  }
  if (hasToken) {
    arguments->emplace_back(std::move(current));
  }
  return true;
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
  int execErrorPipe = -1;
  int execError = 0;
#endif
  bool reaped = false;
  bool exitStatusValid = false;
  int exitCode = 0;
};

void InitializeVideoEncoderProcess() {
#ifndef _WIN32
  struct sigaction action = {};
  action.sa_handler = SIG_IGN;
  sigemptyset(&action.sa_mask);
  sigaction(SIGPIPE, &action, nullptr);
#endif
}

VideoEncoder::VideoEncoder(const VideoEncodeOptions& options, std::string partFile,
                           std::string stagingDirectory)
    : width(options.width), height(options.height), outputFile(options.outputFile),
      partFile(std::move(partFile)), stagingDirectory(std::move(stagingDirectory)) {
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
  std::string partFile = {};
  std::string stagingDirectory = {};
  if (!CreateStagingFile(options.outputFile, &partFile, &stagingDirectory, errorMsg)) {
    return nullptr;
  }
  arguments.back() = partFile;
  std::unique_ptr<VideoEncoder> encoder(
      new VideoEncoder(options, std::move(partFile), std::move(stagingDirectory)));
  if (!encoder->spawnChild(arguments, errorMsg)) {
    auto cleanupError = encoder->abort();
    if (!cleanupError.empty()) {
      auto spawnError = errorMsg != nullptr ? *errorMsg : std::string();
      SetError(errorMsg, spawnError.empty() ? cleanupError : spawnError + "; " + cleanupError);
    }
    return nullptr;
  }
  return encoder;
}

bool VideoEncoder::spawnChild(const std::vector<std::string>& arguments, std::string* errorMsg) {
#ifdef _WIN32
  return spawnChildWindows(arguments, errorMsg);
#else
  return spawnChildPosix(arguments, errorMsg);
#endif
}

#ifdef _WIN32
bool VideoEncoder::spawnChildWindows(const std::vector<std::string>& arguments,
                                     std::string* errorMsg) {
  auto process = std::make_unique<ChildProcess>();
  auto applicationName = UTF8ToWide(arguments.front());
  if (applicationName.empty()) {
    SetError(errorMsg, "failed to encode the ffmpeg path as UTF-16");
    return false;
  }
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
  if (!CreateProcessW(applicationName.c_str(), commandLine.data(), nullptr, nullptr, TRUE, 0,
                      nullptr, nullptr, &startup, &info)) {
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
}
#else
bool VideoEncoder::spawnChildPosix(const std::vector<std::string>& arguments,
                                   std::string* errorMsg) {
  auto process = std::make_unique<ChildProcess>();
  int fds[2] = {-1, -1};
  if (pipe(fds) != 0) {
    SetError(errorMsg,
             "failed to create the ffmpeg input pipe: " + std::string(std::strerror(errno)));
    return false;
  }
  // A later encoder must not inherit an earlier encoder's stdin writer, or the earlier ffmpeg will
  // never observe EOF while both processes are alive.
  if (fcntl(fds[0], F_SETFD, FD_CLOEXEC) != 0 || fcntl(fds[1], F_SETFD, FD_CLOEXEC) != 0) {
    auto error = errno;
    close(fds[0]);
    close(fds[1]);
    SetError(errorMsg,
             "failed to configure the ffmpeg input pipe: " + std::string(std::strerror(error)));
    return false;
  }
  int execErrors[2] = {-1, -1};
  if (pipe(execErrors) != 0) {
    auto error = errno;
    close(fds[0]);
    close(fds[1]);
    SetError(errorMsg,
             "failed to create the exec error pipe: " + std::string(std::strerror(error)));
    return false;
  }
  if (fcntl(execErrors[0], F_SETFD, FD_CLOEXEC) != 0 ||
      fcntl(execErrors[1], F_SETFD, FD_CLOEXEC) != 0) {
    auto error = errno;
    close(fds[0]);
    close(fds[1]);
    close(execErrors[0]);
    close(execErrors[1]);
    SetError(errorMsg,
             "failed to configure the exec error pipe: " + std::string(std::strerror(error)));
    return false;
  }
  std::vector<char*> argv = {};
  argv.reserve(arguments.size() + 1);
  for (const auto& argument : arguments) {
    argv.emplace_back(const_cast<char*>(argument.c_str()));
  }
  argv.emplace_back(nullptr);
  auto processId = fork();
  if (processId < 0) {
    auto error = errno;
    close(fds[0]);
    close(fds[1]);
    close(execErrors[0]);
    close(execErrors[1]);
    SetError(errorMsg, "failed to fork the ffmpeg process: " + std::string(std::strerror(error)));
    return false;
  }
  if (processId == 0) {
    close(execErrors[0]);
    struct sigaction defaultPipe = {};
    defaultPipe.sa_handler = SIG_DFL;
    sigemptyset(&defaultPipe.sa_mask);
    bool inputReady = dup2(fds[0], STDIN_FILENO) >= 0 && fcntl(STDIN_FILENO, F_SETFD, 0) >= 0;
    if (!inputReady || sigaction(SIGPIPE, &defaultPipe, nullptr) != 0) {
      int error = errno;
      write(execErrors[1], &error, sizeof(error));
      _exit(127);
    }
    if (fds[0] != STDIN_FILENO) {
      close(fds[0]);
    }
    close(fds[1]);
    execv(argv[0], argv.data());
    int error = errno;
    write(execErrors[1], &error, sizeof(error));
    _exit(127);
  }
  close(fds[0]);
  close(execErrors[1]);
  process->processId = processId;
  process->stdinPipe = fds[1];
  process->execErrorPipe = execErrors[0];
  child = std::move(process);
  return true;
}
#endif

bool VideoEncoder::writeFrame(const uint8_t* pixels, size_t rowBytes, std::string* errorMsg) {
  if (stopped || child == nullptr) {
    SetError(errorMsg, "the encoder is not running");
    return false;
  }
  if (pixels == nullptr) {
    SetError(errorMsg, "the frame buffer is null");
    return false;
  }
  auto widthSize = static_cast<size_t>(width);
  auto heightSize = static_cast<size_t>(height);
  if (widthSize > std::numeric_limits<size_t>::max() / 4) {
    SetError(errorMsg, "the frame row size overflows size_t");
    return false;
  }
  auto packedRowBytes = widthSize * 4;
  if (rowBytes < packedRowBytes) {
    SetError(errorMsg, "the row stride " + std::to_string(rowBytes) +
                           " is smaller than width * 4 (" + std::to_string(packedRowBytes) + ")");
    return false;
  }
  if (rowBytes > std::numeric_limits<size_t>::max() / heightSize ||
      packedRowBytes > std::numeric_limits<size_t>::max() / heightSize) {
    SetError(errorMsg, "the frame byte size overflows size_t");
    return false;
  }
  const uint8_t* data = pixels;
  size_t size = rowBytes * heightSize;
  if (rowBytes != packedRowBytes) {
    packedBuffer.resize(packedRowBytes * heightSize);
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
    auto succeeded = WriteFile(child->stdinPipe, data + offset, chunk, &written, nullptr) != 0;
    if (succeeded && written > 0) {
      offset += written;
      continue;
    }
    auto error = succeeded ? ERROR_WRITE_FAULT : GetLastError();
    return handleWriteFailure(error, error == ERROR_BROKEN_PIPE || error == ERROR_NO_DATA,
                              errorMsg);
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
    auto error = written == 0 ? EIO : errno;
    return handleWriteFailure(error, error == EPIPE, errorMsg);
  }
  return true;
#endif
}

bool VideoEncoder::handleWriteFailure(uint32_t error, bool brokenPipe, std::string* errorMsg) {
  closeStdin();
  int exitCode = 0;
  auto hasExitStatus = tryReapChild(&exitCode);
  auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(100);
  while (brokenPipe && child != nullptr && !child->reaped &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    hasExitStatus = tryReapChild(&exitCode);
  }
  if (child != nullptr && !child->reaped) {
    killChild();
  }
  if (child != nullptr && child->reaped) {
    hasExitStatus = child->exitStatusValid;
    exitCode = child->exitCode;
  }
  stopped = true;
  auto cleanupError = removePartFile();
  std::string message = {};
  if (brokenPipe) {
    message = FrameWriteFailure(framesWritten, exitCode);
    if (!hasExitStatus) {
      message += " (exit status unavailable)";
    }
  } else {
#ifdef _WIN32
    message = "failed to write a frame to ffmpeg (Windows error " + std::to_string(error) + ")";
#else
    message = "failed to write a frame to ffmpeg: " + std::string(std::strerror(error));
#endif
  }
#ifndef _WIN32
  if (child != nullptr) {
    message += ExecErrorSuffix(child->execError);
  }
#endif
  if (!cleanupError.empty()) {
    message += "; " + cleanupError;
  }
  SetError(errorMsg, message);
  return false;
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

bool VideoEncoder::reapChild(int* exitCode) {
  return reapChildInternal(exitCode, true);
}

bool VideoEncoder::tryReapChild(int* exitCode) {
  return reapChildInternal(exitCode, false);
}

void VideoEncoder::readExecError() {
#ifndef _WIN32
  if (child == nullptr || child->execErrorPipe < 0) {
    return;
  }
  int error = 0;
  ssize_t bytesRead = 0;
  do {
    bytesRead = read(child->execErrorPipe, &error, sizeof(error));
  } while (bytesRead < 0 && errno == EINTR);
  if (bytesRead == static_cast<ssize_t>(sizeof(error))) {
    child->execError = error;
  }
  close(child->execErrorPipe);
  child->execErrorPipe = -1;
#endif
}

bool VideoEncoder::reapChildInternal(int* exitCode, bool wait) {
  if (child == nullptr || exitCode == nullptr) {
    return false;
  }
  *exitCode = child->exitCode;
  if (child->reaped) {
    return child->exitStatusValid;
  }
#ifdef _WIN32
  auto waitResult = WaitForSingleObject(child->process, wait ? INFINITE : 0);
  if (waitResult != WAIT_OBJECT_0) {
    return false;
  }
  DWORD code = 0;
  auto hasExitStatus = GetExitCodeProcess(child->process, &code) != 0;
  CloseHandle(child->process);
  child->process = nullptr;
  if (child->thread != nullptr) {
    CloseHandle(child->thread);
    child->thread = nullptr;
  }
  child->reaped = true;
  if (!hasExitStatus) {
    return false;
  }
  child->exitCode = static_cast<int>(code);
#else
  int status = 0;
  pid_t result = -1;
  do {
    result = waitpid(child->processId, &status, wait ? 0 : WNOHANG);
  } while (result < 0 && errno == EINTR);
  if (result == 0) {
    return false;
  }
  if (result < 0) {
    if (errno == ECHILD) {
      child->reaped = true;
      child->exitStatusValid = false;
      child->processId = -1;
      readExecError();
    }
    return false;
  }
  child->reaped = true;
  readExecError();
  if (WIFEXITED(status)) {
    child->exitCode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    child->exitCode = -WTERMSIG(status);
  } else {
    child->exitCode = 1;
  }
#endif
  child->exitStatusValid = true;
  *exitCode = child->exitCode;
  return true;
}

int64_t VideoEncoder::childProcessId() const {
  if (child == nullptr) {
    return -1;
  }
#ifdef _WIN32
  return static_cast<int64_t>(GetProcessId(child->process));
#else
  return child->processId;
#endif
}

bool VideoEncoder::childReaped() const {
  return child == nullptr || child->reaped;
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
  int exitCode = 0;
  reapChild(&exitCode);
}

std::string VideoEncoder::removePartFile() {
  std::string message = {};
  auto fileError = RemoveFileSystemPath(partFile);
  if (fileError) {
    message = "failed to remove temporary output '" + partFile + "': " + fileError.message();
  }
  std::string stagingError = {};
  if (!removeStagingDirectory(&stagingError)) {
    if (!message.empty()) {
      message += "; ";
    }
    message += stagingError;
  }
  return message;
}

bool VideoEncoder::removeStagingDirectory(std::string* errorMsg) {
  if (stagingDirectory.empty()) {
    return true;
  }
#ifdef _WIN32
  auto widePath = UTF8ToWide(stagingDirectory);
  if (widePath.empty()) {
    SetError(errorMsg, "failed to encode the staging directory path as UTF-16");
    return false;
  }
  std::filesystem::path path(widePath);
#else
  std::filesystem::path path(stagingDirectory);
#endif
  std::error_code error = {};
  std::filesystem::remove(path, error);
  if (error) {
    SetError(errorMsg,
             "failed to remove staging directory '" + stagingDirectory + "': " + error.message());
    return false;
  }
  stagingDirectory.clear();
  return true;
}

bool VideoEncoder::finish(std::string* errorMsg) {
  if (stopped || child == nullptr) {
    SetError(errorMsg, "the encoder is not running");
    return false;
  }
  closeStdin();
  int exitCode = 0;
  if (!reapChild(&exitCode)) {
    auto cleanupError = abort();
    std::string message = "failed to obtain the ffmpeg exit status";
    if (!cleanupError.empty()) {
      message += "; " + cleanupError;
    }
    SetError(errorMsg, message);
    return false;
  }
  stopped = true;
  if (exitCode != 0) {
    auto cleanupError = removePartFile();
    auto message = ExitCodeMessage(
        "ffmpeg failed to encode " + std::to_string(framesWritten) + " frame(s)", exitCode);
#ifndef _WIN32
    message += ExecErrorSuffix(child->execError);
#endif
    if (!cleanupError.empty()) {
      message += "; " + cleanupError;
    }
    SetError(errorMsg, message);
    return false;
  }
#ifdef _WIN32
  auto source = UTF8ToWide(partFile);
  auto destination = UTF8ToWide(outputFile);
  if (source.empty() || destination.empty()) {
    SetError(errorMsg, "failed to encode the output paths as UTF-16; temporary file kept at '" +
                           partFile + "'");
    return false;
  }
  if (!MoveFileExW(source.c_str(), destination.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    SetError(errorMsg, "failed to publish '" + outputFile + "' (Windows error " +
                           std::to_string(GetLastError()) + "); temporary file kept at '" +
                           partFile + "'");
    return false;
  }
#else
  std::error_code renameError = {};
  std::filesystem::rename(partFile, outputFile, renameError);
  if (renameError) {
    SetError(errorMsg,
             "failed to move '" + partFile + "' to '" + outputFile + "'; temporary file kept");
    return false;
  }
#endif
  std::string cleanupError = {};
  if (!removeStagingDirectory(&cleanupError)) {
    SetError(errorMsg, "published '" + outputFile + "'; " + cleanupError);
    return false;
  }
  return true;
}

std::string VideoEncoder::abort() {
  if (stopped) {
    return {};
  }
  stopped = true;
  closeStdin();
  killChild();
  return removePartFile();
}

}  // namespace pagx::cli
