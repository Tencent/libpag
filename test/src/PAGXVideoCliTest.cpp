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
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

// This file intentionally keeps the PAGXCliTest fixture name so existing gtest filters and output
// directories continue to select both the general and video CLI tests after the source split.
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "base/PAGTest.h"
#include "cli/CliUtils.h"
#include "cli/CommandExport.h"
#include "cli/VideoEncoder.h"
#include "cli/VideoFrameSource.h"
#include "pagx/PAGXDocument.h"
#include "pagx/VideoExporter.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"
#include "utils/Baseline.h"
#include "utils/CliTestUtils.h"
#include "utils/PAGXTestUtils.h"
#include "utils/ProjectPath.h"

namespace pag {
using namespace tgfx;

//==============================================================================
// Video encoder tests
//==============================================================================

struct ProcessCapture {
  int exitCode = -1;
  std::string output = {};
  std::string errorOutput = {};
};

static std::string ReadBinaryFile(const std::string& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

static ProcessCapture RunCapturedProcess(const std::vector<std::string>& arguments) {
  ProcessCapture result = {};
  if (arguments.empty()) {
    result.errorOutput = "empty process arguments";
    return result;
  }
  static int captureID = 0;
  auto basePath = TempDir() + "/process_capture_" + std::to_string(captureID++);
  auto stdoutPath = basePath + ".stdout";
  auto stderrPath = basePath + ".stderr";
#ifdef _WIN32
  auto wideStdout = pagx::cli::UTF8ToWide(stdoutPath);
  auto wideStderr = pagx::cli::UTF8ToWide(stderrPath);
  SECURITY_ATTRIBUTES security = {};
  security.nLength = sizeof(security);
  security.bInheritHandle = TRUE;
  auto stdoutHandle =
      CreateFileW(wideStdout.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  auto stderrHandle =
      CreateFileW(wideStderr.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &security,
                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (stdoutHandle == INVALID_HANDLE_VALUE || stderrHandle == INVALID_HANDLE_VALUE) {
    if (stdoutHandle != INVALID_HANDLE_VALUE) {
      CloseHandle(stdoutHandle);
    }
    if (stderrHandle != INVALID_HANDLE_VALUE) {
      CloseHandle(stderrHandle);
    }
    result.errorOutput = "failed to create process capture files";
    return result;
  }
  std::wstring commandLine = {};
  for (const auto& argument : arguments) {
    if (!commandLine.empty()) {
      commandLine += L' ';
    }
    commandLine += pagx::cli::QuoteWindowsArg(pagx::cli::UTF8ToWide(argument));
  }
  STARTUPINFOW startup = {};
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startup.hStdOutput = stdoutHandle;
  startup.hStdError = stderrHandle;
  PROCESS_INFORMATION info = {};
  auto created = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr,
                                nullptr, &startup, &info);
  auto createError = created ? ERROR_SUCCESS : GetLastError();
  CloseHandle(stdoutHandle);
  CloseHandle(stderrHandle);
  if (!created) {
    result.errorOutput =
        "failed to start process (Windows error " + std::to_string(createError) + ")";
    return result;
  }
  WaitForSingleObject(info.hProcess, INFINITE);
  DWORD exitCode = 0;
  GetExitCodeProcess(info.hProcess, &exitCode);
  result.exitCode = static_cast<int>(exitCode);
  CloseHandle(info.hProcess);
  CloseHandle(info.hThread);
#else
  auto stdoutFD = open(stdoutPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  auto stderrFD = open(stderrPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (stdoutFD < 0 || stderrFD < 0) {
    if (stdoutFD >= 0) {
      close(stdoutFD);
    }
    if (stderrFD >= 0) {
      close(stderrFD);
    }
    result.errorOutput = "failed to create process capture files";
    return result;
  }
  std::vector<char*> argv = {};
  argv.reserve(arguments.size() + 1);
  for (const auto& argument : arguments) {
    argv.emplace_back(const_cast<char*>(argument.c_str()));
  }
  argv.emplace_back(nullptr);
  auto processID = fork();
  if (processID == 0) {
    if (dup2(stdoutFD, STDOUT_FILENO) >= 0 && dup2(stderrFD, STDERR_FILENO) >= 0) {
      close(stdoutFD);
      close(stderrFD);
      execvp(argv[0], argv.data());
    }
    _exit(127);
  }
  close(stdoutFD);
  close(stderrFD);
  if (processID < 0) {
    result.errorOutput = "failed to fork process: " + std::string(std::strerror(errno));
    return result;
  }
  int status = 0;
  pid_t waitResult = -1;
  do {
    waitResult = waitpid(processID, &status, 0);
  } while (waitResult < 0 && errno == EINTR);
  if (waitResult < 0) {
    result.errorOutput = "failed to wait for process: " + std::string(std::strerror(errno));
    return result;
  }
  if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    result.exitCode = -WTERMSIG(status);
  }
#endif
  result.output = ReadBinaryFile(stdoutPath);
  result.errorOutput = ReadBinaryFile(stderrPath);
  return result;
}

static bool IsExecutableFile(const std::string& path) {
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

static std::string FindSiblingExecutable(const std::string& executable,
                                         const std::string& siblingName) {
  std::filesystem::path path(executable);
#ifdef _WIN32
  path.replace_filename(siblingName + ".exe");
#else
  path.replace_filename(siblingName);
#endif
  return IsExecutableFile(path.string()) ? path.string() : "";
}

#ifndef _WIN32
struct WorkingDirectoryAndPathRestore {
  WorkingDirectoryAndPathRestore() {
    directory = std::filesystem::current_path().string();
    const char* value = std::getenv("PATH");
    path = value != nullptr ? value : "";
    hadPath = value != nullptr;
  }

  ~WorkingDirectoryAndPathRestore() {
    std::error_code ec;
    std::filesystem::current_path(directory, ec);
    if (hadPath) {
      setenv("PATH", path.c_str(), 1);
    } else {
      unsetenv("PATH");
    }
  }

  std::string directory = {};
  std::string path = {};
  bool hadPath = false;
};
#endif

static void WriteExecutable(const std::string& path, const std::string& contents) {
  {
    std::ofstream out(path);
    out << contents;
  }
  std::filesystem::permissions(path, std::filesystem::perms::owner_all);
}

class ScopedEnvironmentVariable {
 public:
  ScopedEnvironmentVariable(const char* name, const std::string& value) : name(name) {
    const auto* current = std::getenv(name);
    hadValue = current != nullptr;
    savedValue = hadValue ? current : "";
    Set(value);
  }

  ~ScopedEnvironmentVariable() {
    Set(hadValue ? savedValue : "");
  }

 private:
  void Set(const std::string& value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value.c_str());
#else
    setenv(name.c_str(), value.c_str(), 1);
#endif
  }

  std::string name = {};
  std::string savedValue = {};
  bool hadValue = false;
};

static pagx::cli::VideoEncodeOptions DefaultVideoOptions() {
  pagx::cli::VideoEncodeOptions options = {};
  options.width = 320;
  options.height = 180;
  options.frameRate = 60;
  options.outputFile = "/tmp/out.mp4";
  return options;
}

static std::vector<std::string> BuildMp4CommandLine(const pagx::cli::VideoEncodeOptions& options,
                                                    std::string* errorMsg) {
  auto* format = pagx::cli::FindVideoFormat("mp4");
  EXPECT_NE(format, nullptr);
  std::vector<std::string> arguments = {};
  auto ok = pagx::cli::BuildFFmpegCommandLine("ffmpeg", *format, options,
                                              options.outputFile + ".part", &arguments, errorMsg);
  EXPECT_TRUE(ok);
  return arguments;
}

static bool HasArgumentSequence(const std::vector<std::string>& arguments,
                                const std::vector<std::string>& expected) {
  if (expected.empty() || arguments.size() < expected.size()) {
    return false;
  }
  for (size_t i = 0; i + expected.size() <= arguments.size(); ++i) {
    if (std::equal(expected.begin(), expected.end(), arguments.begin() + i)) {
      return true;
    }
  }
  return false;
}

CLI_TEST(PAGXCliTest, VideoFormat_OnlyMp4IsSupported) {
  auto* mp4 = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(mp4, nullptr);
  EXPECT_EQ(mp4->container, "mp4");
  EXPECT_EQ(mp4->defaultEncoder, "libx264");
  EXPECT_EQ(mp4->pixelFormat, "yuv420p");
  EXPECT_TRUE(mp4->requiresOpaque);
  EXPECT_TRUE(mp4->requiresEvenDimensions);
  EXPECT_EQ(pagx::cli::FindVideoFormat("webm"), nullptr);
  EXPECT_EQ(pagx::cli::FindVideoFormat("gif"), nullptr);
  EXPECT_EQ(pagx::cli::FindVideoFormat(""), nullptr);
}

CLI_TEST(PAGXCliTest, VideoFrameRateParsing_ChecksExactRationalLimits) {
  pagx::VideoFrameRate frameRate = {};
  ASSERT_TRUE(pagx::cli::ParseVideoFrameRate("30000/1001", &frameRate));
  EXPECT_EQ(frameRate.numerator, 30000);
  EXPECT_EQ(frameRate.denominator, 1001);
  EXPECT_FALSE(
      pagx::cli::ParseVideoFrameRate("999999999999999999/1000000000000000000", &frameRate));
  EXPECT_FALSE(pagx::cli::ParseVideoFrameRate("120000000000000001/1000000000000000", &frameRate));
}

CLI_TEST(PAGXCliTest, VideoCommandLine_BuildsRawvideoPipeline) {
  std::string errorMsg;
  auto arguments = BuildMp4CommandLine(DefaultVideoOptions(), &errorMsg);
  ASSERT_FALSE(arguments.empty());
  EXPECT_EQ(arguments.front(), "ffmpeg");
  EXPECT_TRUE(HasArgumentSequence(arguments, {"-hide_banner", "-loglevel", "error", "-y"}));
  EXPECT_TRUE(HasArgumentSequence(arguments, {"-f", "rawvideo", "-pixel_format", "rgba"}));
  EXPECT_TRUE(HasArgumentSequence(arguments, {"-video_size", "320x180"}));
  EXPECT_TRUE(HasArgumentSequence(arguments, {"-framerate", "60", "-i", "pipe:0"}));
  EXPECT_NE(std::find(arguments.begin(), arguments.end(), "-an"), arguments.end());
  EXPECT_TRUE(
      HasArgumentSequence(arguments, {"-c:v", "libx264", "-preset", "medium", "-crf", "20"}));
  EXPECT_TRUE(HasArgumentSequence(arguments, {"-pix_fmt", "yuv420p"}));
  EXPECT_TRUE(HasArgumentSequence(arguments, {"-colorspace", "bt709", "-color_primaries", "bt709",
                                              "-color_trc", "bt709", "-color_range", "tv"}));
  EXPECT_TRUE(HasArgumentSequence(arguments, {"-movflags", "+faststart", "-f", "mp4"}));
  EXPECT_EQ(arguments[arguments.size() - 2], "mp4");
  EXPECT_EQ(arguments.back(), "/tmp/out.mp4.part");
  auto filterPos = std::find(arguments.begin(), arguments.end(), "-vf");
  ASSERT_NE(filterPos, arguments.end());
  ASSERT_NE(filterPos + 1, arguments.end());
  auto filter = *(filterPos + 1);
  EXPECT_NE(filter.find("in_color_matrix=bt709"), std::string::npos);
  EXPECT_NE(filter.find("out_range=tv"), std::string::npos);
  EXPECT_NE(filter.find("format=yuv420p"), std::string::npos);
}

CLI_TEST(PAGXCliTest, VideoCommandLine_UsesTheExplicitOutputPath) {
  auto options = DefaultVideoOptions();
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);
  std::vector<std::string> arguments = {};
  std::string errorMsg;
  ASSERT_TRUE(pagx::cli::BuildFFmpegCommandLine("ffmpeg", *format, options,
                                                "/tmp/staging/output.mp4", &arguments, &errorMsg));
  EXPECT_EQ(arguments.back(), "/tmp/staging/output.mp4");
  EXPECT_EQ(std::find(arguments.begin(), arguments.end(), options.outputFile + ".part"),
            arguments.end());
}

CLI_TEST(PAGXCliTest, VideoCommandLine_NormalizesFrameRateForFFmpeg) {
  auto options = DefaultVideoOptions();
  options.frameRate = {30000000000000000LL, 1000000000000000LL};
  std::string errorMsg;
  auto arguments = BuildMp4CommandLine(options, &errorMsg);
  EXPECT_TRUE(HasArgumentSequence(arguments, {"-framerate", "30", "-i", "pipe:0"}));

  options.frameRate = {30000000000000001LL, 1000000000000000LL};
  std::vector<std::string> rejectedArguments = {};
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);
  EXPECT_FALSE(pagx::cli::BuildFFmpegCommandLine(
      "ffmpeg", *format, options, options.outputFile + ".part", &rejectedArguments, &errorMsg));
  EXPECT_NE(errorMsg.find("not representable"), std::string::npos);
}

CLI_TEST(PAGXCliTest, VideoCommandLine_RejectsInvalidOptions) {
  std::vector<std::string> arguments = {};
  std::string errorMsg;
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);

  auto options = DefaultVideoOptions();
  options.height = 181;
  EXPECT_FALSE(pagx::cli::BuildFFmpegCommandLine(
      "ffmpeg", *format, options, options.outputFile + ".part", &arguments, &errorMsg));
  EXPECT_NE(errorMsg.find("even"), std::string::npos);

  options = DefaultVideoOptions();
  options.width = 0;
  errorMsg.clear();
  EXPECT_FALSE(pagx::cli::BuildFFmpegCommandLine(
      "ffmpeg", *format, options, options.outputFile + ".part", &arguments, &errorMsg));
  EXPECT_NE(errorMsg.find("size"), std::string::npos);

  options = DefaultVideoOptions();
  options.frameRate = 0;
  errorMsg.clear();
  EXPECT_FALSE(pagx::cli::BuildFFmpegCommandLine(
      "ffmpeg", *format, options, options.outputFile + ".part", &arguments, &errorMsg));
  EXPECT_NE(errorMsg.find("frame rate"), std::string::npos);

  options = DefaultVideoOptions();
  options.crf = 52;
  errorMsg.clear();
  EXPECT_FALSE(pagx::cli::BuildFFmpegCommandLine(
      "ffmpeg", *format, options, options.outputFile + ".part", &arguments, &errorMsg));
  EXPECT_NE(errorMsg.find("crf"), std::string::npos);

  options = DefaultVideoOptions();
  options.outputFile.clear();
  errorMsg.clear();
  EXPECT_FALSE(
      pagx::cli::BuildFFmpegCommandLine("ffmpeg", *format, options, "", &arguments, &errorMsg));
  EXPECT_NE(errorMsg.find("output path"), std::string::npos);

  errorMsg.clear();
  EXPECT_FALSE(pagx::cli::BuildFFmpegCommandLine("", *format, DefaultVideoOptions(),
                                                 "/tmp/out.mp4.part", &arguments, &errorMsg));
  EXPECT_NE(errorMsg.find("ffmpeg path"), std::string::npos);
}

CLI_TEST(PAGXCliTest, VideoCommandLine_HonorsEncoderOverrideAndExtraArgs) {
  auto options = DefaultVideoOptions();
  options.encoder = "h264_videotoolbox";
  options.preset.clear();
  options.crf = -1;
  options.extraArgs = {"-b:v", "6M"};
  std::string errorMsg;
  auto arguments = BuildMp4CommandLine(options, &errorMsg);
  ASSERT_FALSE(arguments.empty());
  EXPECT_TRUE(HasArgumentSequence(arguments, {"-c:v", "h264_videotoolbox"}));
  EXPECT_EQ(std::find(arguments.begin(), arguments.end(), "-preset"), arguments.end());
  EXPECT_EQ(std::find(arguments.begin(), arguments.end(), "-crf"), arguments.end());
  EXPECT_TRUE(HasArgumentSequence(arguments, {"-b:v", "6M", "-movflags", "+faststart"}));
}

CLI_TEST(PAGXCliTest, VideoCommandLine_RejectsNonEncodingExtraArgs) {
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);
  auto options = DefaultVideoOptions();
  options.extraArgs = {"-i", "/tmp/extra-input"};
  std::vector<std::string> arguments = {};
  std::string errorMsg;
  EXPECT_FALSE(pagx::cli::BuildFFmpegCommandLine(
      "ffmpeg", *format, options, options.outputFile + ".part", &arguments, &errorMsg));
  EXPECT_NE(errorMsg.find("unsupported"), std::string::npos);

  options.extraArgs = {"-threads", "2"};
  errorMsg.clear();
  EXPECT_FALSE(pagx::cli::BuildFFmpegCommandLine(
      "ffmpeg", *format, options, options.outputFile + ".part", &arguments, &errorMsg));
  EXPECT_NE(errorMsg.find("unsupported"), std::string::npos);

  options.extraArgs = {"-b:v"};
  errorMsg.clear();
  EXPECT_FALSE(pagx::cli::BuildFFmpegCommandLine(
      "ffmpeg", *format, options, options.outputFile + ".part", &arguments, &errorMsg));
  EXPECT_NE(errorMsg.find("missing value"), std::string::npos);
}

CLI_TEST(PAGXCliTest, VideoSplitArguments_HonorsQuotesAndEmptyArguments) {
  std::vector<std::string> arguments = {};
  std::string errorMsg = {};
  ASSERT_TRUE(pagx::cli::SplitArguments("-b:v  6M \"-x264-params keyint=30\" '-vf scale=1:1' \"\"",
                                        &arguments, &errorMsg));
  ASSERT_EQ(arguments.size(), 5u);
  EXPECT_EQ(arguments[0], "-b:v");
  EXPECT_EQ(arguments[1], "6M");
  EXPECT_EQ(arguments[2], "-x264-params keyint=30");
  EXPECT_EQ(arguments[3], "-vf scale=1:1");
  EXPECT_EQ(arguments[4], "");
  ASSERT_TRUE(pagx::cli::SplitArguments("   ", &arguments, &errorMsg));
  EXPECT_TRUE(arguments.empty());
}

CLI_TEST(PAGXCliTest, VideoSplitArguments_RejectsUnclosedQuotes) {
  std::vector<std::string> arguments = {};
  std::string errorMsg = {};
  EXPECT_FALSE(pagx::cli::SplitArguments("-b:v \"6M", &arguments, &errorMsg));
  EXPECT_TRUE(arguments.empty());
  EXPECT_NE(errorMsg.find("unclosed quote"), std::string::npos);
}

CLI_TEST(PAGXCliTest, VideoFormatCommandLine_QuotesOnlyWhenNeeded) {
  auto line = pagx::cli::FormatCommandLine(
      {"ffmpeg", "-i", "pipe:0", "scale=format=yuv420p", "/tmp/a b.mp4"});
  EXPECT_EQ(line, "ffmpeg -i pipe:0 scale=format=yuv420p \"/tmp/a b.mp4\"");
}

CLI_TEST(PAGXCliTest, VideoFindFFmpeg_RequiresAnExecutable) {
  auto missing = TempDir() + "/no-such-ffmpeg";
  std::filesystem::remove(missing);
  auto location = pagx::cli::FindFFmpeg(missing);
  EXPECT_TRUE(location.path.empty());
  EXPECT_TRUE(location.source.empty());
  EXPECT_NE(location.error.find("--ffmpeg"), std::string::npos);

  location = pagx::cli::FindFFmpeg(TempDir());
  EXPECT_TRUE(location.path.empty());
  EXPECT_NE(location.error.find("not an executable file"), std::string::npos);

  auto fake = TempDir() + "/fake-ffmpeg";
  {
    std::ofstream out(fake);
    out << "#!/bin/sh\n";
  }
  std::filesystem::permissions(fake, std::filesystem::perms::owner_all);
  location = pagx::cli::FindFFmpeg(fake);
  EXPECT_EQ(location.path, fake);
  EXPECT_EQ(location.source, "--ffmpeg");
  EXPECT_TRUE(location.error.empty());
}

CLI_TEST(PAGXCliTest, VideoFindFFmpeg_PrefersManagedCacheAndRejectsInvalidEnvironment) {
#ifdef _WIN32
  const std::string platform = "win32";
#elif defined(__APPLE__)
  const std::string platform = "darwin";
#else
  const std::string platform = "linux";
#endif
#if defined(__x86_64__) || defined(_M_X64)
  const std::string target = platform + "-x64";
#else
  const std::string target = platform + "-arm64";
#endif
#ifdef _WIN32
  const std::string executableName = "ffmpeg.exe";
#else
  const std::string executableName = "ffmpeg";
#endif
  auto cacheRoot = std::filesystem::path(TempDir()) / "managed-cache";
  auto cachedPath = cacheRoot / "v1.0.0" / target / "bin" / executableName;
  std::filesystem::create_directories(cachedPath.parent_path());
  WriteExecutable(cachedPath.u8string(), "#!/bin/sh\n");
  auto manifestPath = std::filesystem::path(TempDir()) / "ffmpeg-manifest.json";
  {
    std::ofstream out(manifestPath);
    out << R"({"release":"v1.0.0","assets":{"darwin-arm64":{},"darwin-x64":{},"linux-arm64":{},"linux-x64":{},"win32-arm64":{},"win32-x64":{}}})";
  }
  auto pathDirectory = std::filesystem::path(TempDir()) / "path-ffmpeg";
  std::filesystem::create_directories(pathDirectory);
  WriteExecutable((pathDirectory / executableName).u8string(), "#!/bin/sh\n");
  ScopedEnvironmentVariable manifestEnv("PAGX_FFMPEG_MANIFEST_PATH", manifestPath.u8string());
  ScopedEnvironmentVariable cacheEnv("PAGX_FFMPEG_CACHE_DIR", cacheRoot.u8string());
  ScopedEnvironmentVariable ffmpegEnv("FFMPEG_BIN", "");
  ScopedEnvironmentVariable pathEnv("PATH", pathDirectory.u8string());

  auto location = pagx::cli::FindFFmpeg("");
  EXPECT_EQ(location.path, cachedPath.u8string());
  EXPECT_EQ(location.source, "managed cache");
  {
    ScopedEnvironmentVariable invalidFFmpeg("FFMPEG_BIN",
                                            (pathDirectory / "missing-ffmpeg").u8string());
    location = pagx::cli::FindFFmpeg("");
    EXPECT_TRUE(location.path.empty());
    EXPECT_NE(location.error.find("FFMPEG_BIN"), std::string::npos);
  }
}

static void FillVideoFrame(std::vector<uint8_t>* frame, size_t stride, int width, int height,
                           int index) {
  frame->assign(stride * static_cast<size_t>(height), 0);
  for (int row = 0; row < height; ++row) {
    auto* pixels = frame->data() + static_cast<size_t>(row) * stride;
    for (int column = 0; column < width; ++column) {
      pixels[column * 4] = static_cast<uint8_t>(index * 20);
      pixels[column * 4 + 1] = static_cast<uint8_t>(column * 3);
      pixels[column * 4 + 2] = static_cast<uint8_t>(row * 5);
      pixels[column * 4 + 3] = 255;
    }
  }
}

static double ComputeFramePSNR(const tgfx::Bitmap& expected, const char* decoded,
                               size_t decodedSize);

static tgfx::Bitmap MakeVideoFrameBitmap(int width, int height, int index) {
  std::vector<uint8_t> pixels = {};
  auto rowBytes = static_cast<size_t>(width) * 4;
  FillVideoFrame(&pixels, rowBytes, width, height, index);
  tgfx::Bitmap bitmap(width, height, false, false);
  tgfx::Pixmap pixmap(bitmap);
  auto* destination = static_cast<uint8_t*>(pixmap.writablePixels());
  for (int row = 0; row < height; ++row) {
    std::memcpy(destination + static_cast<size_t>(row) * pixmap.rowBytes(),
                pixels.data() + static_cast<size_t>(row) * rowBytes, rowBytes);
  }
  return bitmap;
}

CLI_TEST(PAGXCliTest, VideoEncoder_ExecutesTheValidatedRelativePath) {
#ifdef _WIN32
  GTEST_SKIP() << "POSIX relative-path execution semantics";
#else
  auto root = TempDir() + "/relative_ffmpeg";
  std::filesystem::remove_all(root);
  auto currentDirectory = root + "/current";
  auto pathDirectory = root + "/path";
  std::filesystem::create_directories(currentDirectory);
  std::filesystem::create_directories(pathDirectory);
  WriteExecutable(currentDirectory + "/ffmpeg",
                  "#!/bin/sh\n/bin/cat >/dev/null\nfor output in \"$@\"; do :; done\n"
                  ": > \"$output\"\nexit 0\n");
  WriteExecutable(pathDirectory + "/ffmpeg", "#!/bin/sh\nexit 99\n");

  auto* format = pagx::cli::FindVideoFormat("mp4");
  pagx::cli::VideoEncodeOptions options = {};
  options.width = 64;
  options.height = 48;
  options.frameRate = 10;
  options.outputFile = root + "/relative.mp4";
  bool started = false;
  bool wrote = false;
  bool finished = false;
  std::string errorMsg = {};
  {
    WorkingDirectoryAndPathRestore restore;
    setenv("PATH", pathDirectory.c_str(), 1);
    std::filesystem::current_path(currentDirectory);
    auto location = pagx::cli::FindFFmpeg("ffmpeg");
    auto encoder = pagx::cli::VideoEncoder::Start(location.path, *format, options, &errorMsg);
    started = encoder != nullptr;
    if (started) {
      std::vector<uint8_t> frame = {};
      FillVideoFrame(&frame, 64 * 4, 64, 48, 0);
      wrote = encoder->writeFrame(frame.data(), 64 * 4, &errorMsg);
      finished = encoder->finish(&errorMsg);
    }
  }
  EXPECT_TRUE(started) << errorMsg;
  EXPECT_TRUE(wrote) << errorMsg;
  EXPECT_TRUE(finished) << errorMsg;
  EXPECT_TRUE(std::filesystem::exists(options.outputFile));
#endif
}

CLI_TEST(PAGXCliTest, VideoEncoder_EncodesPaddedFramesWhenFFmpegIsAvailable) {
  auto location = pagx::cli::FindFFmpeg("");
  if (location.path.empty()) {
    GTEST_SKIP() << "ffmpeg is not available";
  }
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);
  const int width = 64;
  const int height = 48;
  const int frameCount = 10;
  const size_t stride = static_cast<size_t>(width) * 4 + 64;
  pagx::cli::VideoEncodeOptions options = {};
  options.width = width;
  options.height = height;
  options.frameRate = 10;
  options.outputFile = TempDir() + "/encoder_padded.mp4";
  std::filesystem::remove(options.outputFile);
  std::filesystem::remove(options.outputFile + ".part");

  std::string errorMsg;
  auto encoder = pagx::cli::VideoEncoder::Start(location.path, *format, options, &errorMsg);
  ASSERT_NE(encoder, nullptr) << errorMsg;
  std::vector<uint8_t> frame = {};
  for (int index = 0; index < frameCount; ++index) {
    FillVideoFrame(&frame, stride, width, height, index);
    errorMsg.clear();
    ASSERT_TRUE(encoder->writeFrame(frame.data(), stride, &errorMsg)) << errorMsg;
  }
  errorMsg.clear();
  ASSERT_TRUE(encoder->finish(&errorMsg)) << errorMsg;
  EXPECT_FALSE(std::filesystem::exists(options.outputFile + ".part"));
  EXPECT_FALSE(std::filesystem::exists(encoder->stagingDirectory));
  ASSERT_TRUE(std::filesystem::exists(options.outputFile));
  EXPECT_GT(std::filesystem::file_size(options.outputFile), 1000u);

  std::ifstream in(options.outputFile, std::ios::binary);
  std::vector<char> header(8, 0);
  in.read(header.data(), 8);
  EXPECT_EQ(std::string(header.data() + 4, 4), "ftyp");

  auto decoded = RunCapturedProcess(
      {location.path, "-hide_banner", "-loglevel", "error", "-i", options.outputFile, "-vf",
       "scale=in_color_matrix=bt709:in_range=tv:out_range=pc,format=rgba", "-fps_mode",
       "passthrough", "-f", "rawvideo", "pipe:1"});
  ASSERT_EQ(decoded.exitCode, 0) << decoded.errorOutput;
  const size_t frameBytes = static_cast<size_t>(width) * height * 4;
  ASSERT_EQ(decoded.output.size(), frameBytes * frameCount);
  for (int index = 0; index < frameCount; ++index) {
    auto expected = MakeVideoFrameBitmap(width, height, index);
    auto psnr = ComputeFramePSNR(expected, decoded.output.data() + index * frameBytes, frameBytes);
    EXPECT_GT(psnr, 30.0) << "frame " << index;
  }
}

CLI_TEST(PAGXCliTest, VideoEncoder_UsesDistinctTemporaryFilesForTheSameOutput) {
  auto location = pagx::cli::FindFFmpeg("");
  if (location.path.empty()) {
    GTEST_SKIP() << "ffmpeg is not available";
  }
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);
  pagx::cli::VideoEncodeOptions options = {};
  options.width = 64;
  options.height = 48;
  options.frameRate = 10;
  options.outputFile = TempDir() + "/encoder_shared_output.mp4";
  std::filesystem::remove(options.outputFile);

  std::string errorMsg;
  auto first = pagx::cli::VideoEncoder::Start(location.path, *format, options, &errorMsg);
  ASSERT_NE(first, nullptr) << errorMsg;
  errorMsg.clear();
  auto second = pagx::cli::VideoEncoder::Start(location.path, *format, options, &errorMsg);
  ASSERT_NE(second, nullptr) << errorMsg;

  std::vector<uint8_t> frame = {};
  FillVideoFrame(&frame, 64 * 4, 64, 48, 0);
  ASSERT_TRUE(first->writeFrame(frame.data(), frame.size() / 48, &errorMsg)) << errorMsg;
  ASSERT_TRUE(second->writeFrame(frame.data(), frame.size() / 48, &errorMsg)) << errorMsg;
  ASSERT_TRUE(first->finish(&errorMsg)) << errorMsg;
  ASSERT_TRUE(second->finish(&errorMsg)) << errorMsg;
  EXPECT_TRUE(std::filesystem::exists(options.outputFile));
}

CLI_TEST(PAGXCliTest, VideoEncoder_FailurePreservesAnUnrelatedFixedPartFile) {
  auto location = pagx::cli::FindFFmpeg("");
  if (location.path.empty()) {
    GTEST_SKIP() << "ffmpeg is not available";
  }
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);
  const int width = 64;
  const int height = 48;
  const size_t stride = static_cast<size_t>(width) * 4;
  pagx::cli::VideoEncodeOptions options = {};
  options.width = width;
  options.height = height;
  options.frameRate = 10;
  options.encoder = "libunknownencoder";
  options.outputFile = TempDir() + "/encoder_failure.mp4";
  std::filesystem::remove(options.outputFile);
  {
    std::ofstream out(options.outputFile + ".part");
    out << "unrelated data";
  }

  std::string errorMsg;
  auto encoder = pagx::cli::VideoEncoder::Start(location.path, *format, options, &errorMsg);
  ASSERT_NE(encoder, nullptr) << errorMsg;
  std::vector<uint8_t> frame = {};
  FillVideoFrame(&frame, stride, width, height, 0);
  std::string writeError;
  auto wrote = encoder->writeFrame(frame.data(), stride, &writeError);
  std::string finishError;
  auto finished = encoder->finish(&finishError);
  EXPECT_FALSE(finished);
  if (wrote) {
    EXPECT_NE(finishError.find("ffmpeg failed"), std::string::npos) << finishError;
  } else {
    EXPECT_FALSE(writeError.empty());
    EXPECT_NE(finishError.find("not running"), std::string::npos) << finishError;
  }
  EXPECT_FALSE(std::filesystem::exists(options.outputFile));
  EXPECT_EQ(ReadTextFile(options.outputFile + ".part"), "unrelated data");
}

CLI_TEST(PAGXCliTest, VideoEncoder_HandlesAnEarlyClosedPipe) {
#ifdef _WIN32
  GTEST_SKIP() << "POSIX controlled process regression";
#else
  auto script = TempDir() + "/close-stdin.sh";
  auto readyFile = TempDir() + "/stdin-closed";
  std::filesystem::remove(readyFile);
  WriteExecutable(script, "#!/bin/sh\nexec 0<&-\nprintf ready > \"" + readyFile + "\"\nexit 1\n");
  auto options = DefaultVideoOptions();
  options.outputFile = TempDir() + "/early_closed_pipe.mp4";
  {
    std::ofstream out(options.outputFile + ".part");
    out << "unrelated data";
  }
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);
  std::string errorMsg;
  auto encoder = pagx::cli::VideoEncoder::Start(script, *format, options, &errorMsg);
  ASSERT_NE(encoder, nullptr) << errorMsg;
  for (int attempt = 0; attempt < 1000 && !std::filesystem::exists(readyFile); ++attempt) {
    usleep(1000);
  }
  ASSERT_TRUE(std::filesystem::exists(readyFile));
  std::vector<uint8_t> frame = {};
  FillVideoFrame(&frame, 320 * 4, 320, 180, 0);
  EXPECT_FALSE(encoder->writeFrame(frame.data(), 320 * 4, &errorMsg));
  EXPECT_NE(errorMsg.find("stopped reading"), std::string::npos) << errorMsg;
  EXPECT_NE(errorMsg.find("exit code 1"), std::string::npos) << errorMsg;
  EXPECT_EQ(errorMsg.find("exit status unavailable"), std::string::npos) << errorMsg;
  EXPECT_FALSE(encoder->finish(&errorMsg));
  EXPECT_FALSE(std::filesystem::exists(options.outputFile));
  EXPECT_FALSE(std::filesystem::exists(encoder->stagingDirectory));
  EXPECT_EQ(ReadTextFile(options.outputFile + ".part"), "unrelated data");
#endif
}

CLI_TEST(PAGXCliTest, VideoEncoder_ReportsConsumerExitStatus) {
#ifdef _WIN32
  GTEST_SKIP() << "POSIX controlled process regression";
#else
  auto script = TempDir() + "/consume-and-fail.sh";
  WriteExecutable(script, "#!/bin/sh\ncat >/dev/null\nexit 7\n");
  auto options = DefaultVideoOptions();
  options.outputFile = TempDir() + "/consumer_exit.mp4";
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);
  std::string errorMsg;
  auto encoder = pagx::cli::VideoEncoder::Start(script, *format, options, &errorMsg);
  ASSERT_NE(encoder, nullptr) << errorMsg;
  std::vector<uint8_t> frame = {};
  FillVideoFrame(&frame, 320 * 4, 320, 180, 0);
  ASSERT_TRUE(encoder->writeFrame(frame.data(), 320 * 4, &errorMsg)) << errorMsg;
  EXPECT_FALSE(encoder->finish(&errorMsg));
  EXPECT_NE(errorMsg.find("exit code 7"), std::string::npos) << errorMsg;
  EXPECT_FALSE(std::filesystem::exists(options.outputFile));
  EXPECT_FALSE(std::filesystem::exists(encoder->stagingDirectory));
#endif
}

CLI_TEST(PAGXCliTest, VideoEncoder_ReportsExecFailureAndPreservesFixedPartFile) {
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);
  pagx::cli::VideoEncodeOptions options = {};
  options.width = 64;
  options.height = 48;
  options.frameRate = 10;
  options.outputFile = TempDir() + "/encoder_exec_failure.mp4";
  std::filesystem::remove(options.outputFile);
  {
    std::ofstream out(options.outputFile + ".part");
    out << "unrelated data";
  }
  auto missingExecutable = TempDir() + "/missing-ffmpeg";
  std::filesystem::remove(missingExecutable);
  std::string errorMsg = {};
  auto encoder = pagx::cli::VideoEncoder::Start(missingExecutable, *format, options, &errorMsg);
#ifdef _WIN32
  ASSERT_EQ(encoder, nullptr);
  EXPECT_FALSE(errorMsg.empty());
#else
  ASSERT_NE(encoder, nullptr) << errorMsg;
  std::vector<uint8_t> frame = {};
  FillVideoFrame(&frame, 64 * 4, 64, 48, 0);
  if (encoder->writeFrame(frame.data(), 64 * 4, &errorMsg)) {
    EXPECT_FALSE(encoder->finish(&errorMsg));
  }
  EXPECT_NE(errorMsg.find("127"), std::string::npos) << errorMsg;
  EXPECT_NE(errorMsg.find("failed to execute ffmpeg"), std::string::npos) << errorMsg;
  EXPECT_FALSE(std::filesystem::exists(encoder->stagingDirectory));
#endif
  EXPECT_FALSE(std::filesystem::exists(options.outputFile));
  EXPECT_EQ(ReadTextFile(options.outputFile + ".part"), "unrelated data");
}

CLI_TEST(PAGXCliTest, VideoEncoder_DoesNotKillAnExternallyReapedProcess) {
#ifdef _WIN32
  GTEST_SKIP() << "POSIX process state regression";
#else
  auto location = pagx::cli::FindFFmpeg("");
  if (location.path.empty()) {
    GTEST_SKIP() << "ffmpeg is not available";
  }
  auto* format = pagx::cli::FindVideoFormat("mp4");
  ASSERT_NE(format, nullptr);
  auto options = DefaultVideoOptions();
  options.outputFile = TempDir() + "/externally_reaped.mp4";
  std::string errorMsg;
  auto encoder = pagx::cli::VideoEncoder::Start(location.path, *format, options, &errorMsg);
  ASSERT_NE(encoder, nullptr) << errorMsg;
  auto processId = encoder->childProcessId();
  ASSERT_GT(processId, 0);
  ASSERT_EQ(kill(processId, SIGKILL), 0);
  int status = 0;
  while (waitpid(processId, &status, 0) < 0 && errno == EINTR) {
  }
  int exitCode = 0;
  EXPECT_FALSE(encoder->reapChild(&exitCode));
  EXPECT_TRUE(encoder->childReaped());
  EXPECT_EQ(encoder->childProcessId(), -1);
  encoder->killChild();
  encoder->abort();
#endif
}

CLI_TEST(PAGXCliTest, Export_Mp4DryRunAcceptsStaticDocument) {
  auto inputPath = TempDir() + "/static_input.pagx";
  auto outputPath = TempDir() + "/static_output.mp4";
  {
    std::ofstream out(inputPath);
    out << "<pagx width=\"64\" height=\"48\">\n"
           "  <Layer>\n"
           "    <Rectangle position=\"32,24\" size=\"64,48\"/>\n"
           "    <Fill color=\"#FF0000\"/>\n"
           "  </Layer>\n"
           "</pagx>\n";
  }
  std::string output;
  int ret = 0;
  {
    StreamCapture capturedOut(std::cout);
    ret = CallRun(pagx::cli::RunExport,
                  {"export", "--input", inputPath, "--output", outputPath, "--dry-run"});
    output = capturedOut.str();
  }
  EXPECT_EQ(ret, 0);
  EXPECT_NE(output.find(" -framerate 60 "), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(outputPath));
}

CLI_TEST(PAGXCliTest, Export_Mp4DryRunDoesNotRequireAnExecutable) {
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  auto outputPath = TempDir() + "/dry_run_without_ffmpeg.mp4";
  auto missingExecutable = TempDir() + "/missing-ffmpeg";
  std::filesystem::remove(missingExecutable);
  std::string output;
  int ret = 0;
  {
    StreamCapture capturedOut(std::cout);
    ret = CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath,
                                         "--ffmpeg", missingExecutable, "--dry-run"});
    output = capturedOut.str();
  }
  EXPECT_EQ(ret, 0);
  EXPECT_NE(output.find(missingExecutable), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(outputPath));
}

CLI_TEST(PAGXCliTest, Export_Mp4DryRunDownscalesToTheGPUTextureLimit) {
  auto inputPath = TempDir() + "/oversized_input.pagx";
  auto outputPath = TempDir() + "/oversized_output.mp4";
  {
    std::ofstream out(inputPath);
    out << "<pagx width=\"100000\" height=\"100\">\n"
           "  <Layer>\n"
           "    <Rectangle position=\"50000,50\" size=\"100000,100\"/>\n"
           "    <Fill color=\"#FF0000\"/>\n"
           "  </Layer>\n"
           "</pagx>\n";
  }
  std::string output;
  std::string errors;
  int ret = 0;
  {
    StreamCapture capturedOut(std::cout);
    StreamCapture capturedErr(std::cerr);
    ret = CallRun(pagx::cli::RunExport,
                  {"export", "--input", inputPath, "--output", outputPath, "--dry-run"});
    output = capturedOut.str();
    errors = capturedErr.str();
  }
  ASSERT_EQ(ret, 0) << errors;
  EXPECT_NE(errors.find("maxTextureDimension2D"), std::string::npos);
  EXPECT_NE(errors.find("downscaling"), std::string::npos);
  EXPECT_EQ(output.find("-video_size 100000x100"), std::string::npos);
  EXPECT_NE(output.find("-video_size "), std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_Mp4DryRunPrintsTheCommandLineAndWritesNothing) {
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  auto outputPath = TempDir() + "/export_dry_run.mp4";
  std::filesystem::remove(outputPath);
  std::filesystem::remove(outputPath + ".part");
  std::string output;
  int ret = 0;
  {
    StreamCapture capturedOut(std::cout);
    ret = CallRun(pagx::cli::RunExport,
                  {"export", "--input", inputPath, "--output", outputPath, "--dry-run"});
    output = capturedOut.str();
  }
  EXPECT_EQ(ret, 0);
  EXPECT_NE(output.find("-f rawvideo -pixel_format rgba"), std::string::npos);
  EXPECT_NE(output.find("-video_size 300x100"), std::string::npos);
  EXPECT_NE(output.find("-framerate 60"), std::string::npos);
  EXPECT_NE(output.find(outputPath + ".part"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(outputPath));
  EXPECT_FALSE(std::filesystem::exists(outputPath + ".part"));
}

CLI_TEST(PAGXCliTest, Export_Mp4DryRunPreservesFractionalFrameRate) {
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  auto outputPath = TempDir() + "/export_fractional_fps.mp4";
  std::string output;
  int ret = 0;
  {
    StreamCapture capturedOut(std::cout);
    ret = CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath,
                                         "--fps", "30000/1001", "--dry-run"});
    output = capturedOut.str();
  }
  EXPECT_EQ(ret, 0);
  EXPECT_NE(output.find("-framerate 30000/1001"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(outputPath));
}

CLI_TEST(PAGXCliTest, Export_Mp4RoundsAnOddSizeUpToEven) {
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  auto outputPath = TempDir() + "/export_odd_size.mp4";
  std::string output;
  std::string errors;
  int ret = 0;
  {
    StreamCapture capturedOut(std::cout);
    StreamCapture capturedErr(std::cerr);
    ret = CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath,
                                         "--scale", "1.003", "--dry-run"});
    output = capturedOut.str();
    errors = capturedErr.str();
  }
  EXPECT_EQ(ret, 0);
  EXPECT_NE(errors.find("rounded up to 302x102"), std::string::npos);
  EXPECT_NE(output.find("-video_size 302x102"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_VideoOptionsAreRejectedForVectorFormats) {
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  std::string errors;
  int ret = 0;
  {
    StreamCapture capturedErr(std::cerr);
    ret = CallRun(pagx::cli::RunExport,
                  {"export", "--input", inputPath, "--output", TempDir() + "/video_options.svg",
                   "--fps", "30", "--duration", "1"});
    errors = capturedErr.str();
  }
  EXPECT_EQ(ret, 1);
  EXPECT_NE(errors.find("--fps is only supported for video formats"), std::string::npos);
  {
    StreamCapture capturedErr(std::cerr);
    ret = CallRun(pagx::cli::RunExport,
                  {"export", "--input", inputPath, "--output", TempDir() + "/video_options.svg",
                   "--duration", "1", "--fps", "30"});
    errors = capturedErr.str();
  }
  EXPECT_EQ(ret, 1);
  EXPECT_NE(errors.find("--duration is only supported for video formats"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_FontOptionsAreRejectedForNonVideoFormats) {
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  for (const auto* format : {"svg", "pptx", "html"}) {
    for (const auto* option : {"--font", "--fallback"}) {
      std::string errors;
      int ret = 0;
      {
        StreamCapture capturedErr(std::cerr);
        ret = CallRun(pagx::cli::RunExport,
                      {"export", "--input", inputPath, "--output",
                       TempDir() + "/font_options." + format, option, "missing.ttf"});
        errors = capturedErr.str();
      }
      EXPECT_EQ(ret, 1);
      EXPECT_NE(errors.find(std::string(option) + " is only supported for video formats"),
                std::string::npos);
    }
  }
}

CLI_TEST(PAGXCliTest, Export_Mp4RejectsUnclosedEncoderArguments) {
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  std::string errors;
  int ret = 0;
  {
    StreamCapture capturedErr(std::cerr);
    ret = CallRun(pagx::cli::RunExport,
                  {"export", "--input", inputPath, "--output", TempDir() + "/invalid_args.mp4",
                   "--encoder-args", "\"-b:v"});
    errors = capturedErr.str();
  }
  EXPECT_EQ(ret, 1);
  EXPECT_NE(errors.find("unclosed quote"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_Mp4RejectsATransparentBackground) {
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  std::string errors;
  int ret = 0;
  {
    StreamCapture capturedErr(std::cerr);
    ret = CallRun(pagx::cli::RunExport,
                  {"export", "--input", inputPath, "--output",
                   TempDir() + "/transparent_background.mp4", "--background", "#FFFFFF00"});
    errors = capturedErr.str();
  }
  EXPECT_EQ(ret, 1);
  EXPECT_NE(errors.find("cannot carry alpha"), std::string::npos);
}

CLI_TEST(PAGXCliTest, Export_Mp4EncodesTheAnimationWhenFFmpegIsAvailable) {
  if (pagx::cli::FindFFmpeg("").path.empty()) {
    GTEST_SKIP() << "ffmpeg is not available";
  }
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  auto outputPath = TempDir() + "/export_animation.mp4";
  std::filesystem::remove(outputPath);
  std::filesystem::remove(outputPath + ".part");
  std::string output;
  int ret = 0;
  {
    StreamCapture capturedOut(std::cout);
    ret = CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath,
                                         "--fps", "20", "--crf", "30"});
    output = capturedOut.str();
  }
  EXPECT_EQ(ret, 0);
  EXPECT_NE(output.find("wrote " + outputPath), std::string::npos);
  EXPECT_NE(output.find("8 frames"), std::string::npos);
  EXPECT_FALSE(std::filesystem::exists(outputPath + ".part"));
  ASSERT_TRUE(std::filesystem::exists(outputPath));
  EXPECT_GT(std::filesystem::file_size(outputPath), 1000u);
}

static std::string ProbeValue(const std::string& output, const std::string& key) {
  std::istringstream lines(output);
  std::string line = {};
  auto prefix = key + "=";
  while (std::getline(lines, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (line.rfind(prefix, 0) == 0) {
      return line.substr(prefix.size());
    }
  }
  return {};
}

static double ComputeFramePSNR(const tgfx::Bitmap& expected, const char* decoded,
                               size_t decodedSize) {
  auto frameBytes = static_cast<size_t>(expected.width()) * expected.height() * 4;
  if (decodedSize != frameBytes) {
    return 0.0;
  }
  tgfx::Pixmap expectedPixels(expected);
  auto* expectedData = static_cast<const uint8_t*>(expectedPixels.pixels());
  auto* decodedData = reinterpret_cast<const uint8_t*>(decoded);
  double squaredError = 0.0;
  for (int row = 0; row < expected.height(); ++row) {
    auto* expectedRow = expectedData + static_cast<size_t>(row) * expectedPixels.rowBytes();
    auto* decodedRow = decodedData + static_cast<size_t>(row) * expected.width() * 4;
    for (int column = 0; column < expected.width() * 4; column += 4) {
      for (int channel = 0; channel < 3; ++channel) {
        auto difference = static_cast<double>(expectedRow[column + channel]) -
                          static_cast<double>(decodedRow[column + channel]);
        squaredError += difference * difference;
      }
    }
  }
  auto meanSquaredError =
      squaredError / (static_cast<double>(expected.width()) * expected.height() * 3);
  if (meanSquaredError == 0.0) {
    return 99.0;
  }
  return 10.0 * std::log10(255.0 * 255.0 / meanSquaredError);
}

CLI_TEST(PAGXCliTest, Export_Mp4MetadataAndDecodedFramesMatch) {
  auto location = pagx::cli::FindFFmpeg("");
  if (location.path.empty()) {
    GTEST_SKIP() << "ffmpeg is not available";
  }
  auto ffprobe = FindSiblingExecutable(location.path, "ffprobe");
  if (ffprobe.empty()) {
    GTEST_SKIP() << "ffprobe is not available next to ffmpeg";
  }
  auto inputPath = TestResourcePath("render_animated_timelines.pagx");
  auto outputPath = TempDir() + "/export_media_inspection.mp4";
  std::filesystem::remove(outputPath);
  auto ret = CallRun(pagx::cli::RunExport, {"export", "--input", inputPath, "--output", outputPath,
                                            "--fps", "20", "--crf", "30"});
  ASSERT_EQ(ret, 0);

  auto metadata = RunCapturedProcess(
      {ffprobe, "-v", "error", "-select_streams", "v:0", "-show_entries",
       "stream=codec_name,width,height,pix_fmt,r_frame_rate,nb_frames:format=duration", "-of",
       "default=noprint_wrappers=1", outputPath});
  ASSERT_EQ(metadata.exitCode, 0) << metadata.errorOutput;
  EXPECT_EQ(ProbeValue(metadata.output, "codec_name"), "h264");
  EXPECT_EQ(ProbeValue(metadata.output, "width"), "300");
  EXPECT_EQ(ProbeValue(metadata.output, "height"), "100");
  EXPECT_EQ(ProbeValue(metadata.output, "pix_fmt"), "yuv420p");
  EXPECT_EQ(ProbeValue(metadata.output, "r_frame_rate"), "20/1");
  // The nested 20-frame animation has a 5-frame evaluation offset, so 25/60 seconds rounds to
  // eight frames at 20 fps.
  EXPECT_EQ(ProbeValue(metadata.output, "nb_frames"), "8");
  auto durationString = ProbeValue(metadata.output, "duration");
  char* durationEnd = nullptr;
  auto duration = std::strtof(durationString.c_str(), &durationEnd);
  EXPECT_NE(durationEnd, durationString.c_str());
  EXPECT_EQ(*durationEnd, '\0');
  EXPECT_NEAR(duration, 0.4f, 0.001f);

  auto audio =
      RunCapturedProcess({ffprobe, "-v", "error", "-select_streams", "a:0", "-show_entries",
                          "stream=index", "-of", "default=noprint_wrappers=1", outputPath});
  ASSERT_EQ(audio.exitCode, 0) << audio.errorOutput;
  EXPECT_TRUE(audio.output.empty()) << audio.output;

  auto decoded =
      RunCapturedProcess({location.path, "-hide_banner", "-loglevel", "error", "-i", outputPath,
                          "-vf", "scale=in_color_matrix=bt709:in_range=tv:out_range=pc,format=rgba",
                          "-fps_mode", "passthrough", "-f", "rawvideo", "pipe:1"});
  ASSERT_EQ(decoded.exitCode, 0) << decoded.errorOutput;
  const char* timeSeconds[] = {"0", "0.05", "0.1", "0.15", "0.2", "0.25", "0.3", "0.35"};
  const size_t frameBytes = 300 * 100 * 4;
  ASSERT_EQ(decoded.output.size(), frameBytes * 8);
  for (int frame = 0; frame < 8; ++frame) {
    auto expected = RenderAnimatedTimelinesAt(timeSeconds[frame], true);
    ASSERT_FALSE(expected.isEmpty());
    auto psnr = ComputeFramePSNR(expected, decoded.output.data() + frame * frameBytes, frameBytes);
    EXPECT_GT(psnr, 30.0) << "frame " << frame;
  }
}

}  // namespace pag
