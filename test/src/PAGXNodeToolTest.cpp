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

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "cli/CommandImport.h"
#include "cli/NodeTool.h"
#include "pagx/PAGXImporter.h"
#include "utils/PAGXTestUtils.h"
#include "utils/ProjectPath.h"
#ifndef _WIN32
#include <sys/wait.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#endif

namespace pag {
namespace {
namespace fs = std::filesystem;

class ScopedEnvironment {
 public:
  ScopedEnvironment(const char* name, const char* value) : name(name) {
    const char* previous = std::getenv(name);
    wasSet = previous != nullptr;
    if (previous) saved = previous;
    set(value);
  }

  ~ScopedEnvironment() {
    set(wasSet ? saved.c_str() : nullptr);
  }

 private:
  void set(const char* value) {
#ifdef _WIN32
    _putenv_s(name.c_str(), value ? value : "");
#else
    if (value) setenv(name.c_str(), value, 1);
    else
      unsetenv(name.c_str());
#endif
  }

  std::string name;
  std::string saved;
  bool wasSet = false;
};

class ScopedCurrentPath {
 public:
  explicit ScopedCurrentPath(const fs::path& path) : previous(fs::current_path()) {
    fs::current_path(path);
  }

  ~ScopedCurrentPath() {
    fs::current_path(previous);
  }

 private:
  fs::path previous;
};

class CaptureError {
 public:
  CaptureError() : previous(std::cerr.rdbuf(output.rdbuf())) {
  }
  ~CaptureError() {
    std::cerr.rdbuf(previous);
  }
  std::string str() const {
    return output.str();
  }

 private:
  std::ostringstream output;
  std::streambuf* previous;
};

class PAGXNodeToolTest : public testing::Test {
 protected:
  void SetUp() override {
    auto id = std::chrono::steady_clock::now().time_since_epoch().count();
    auto directory =
        fs::canonical(fs::temp_directory_path()) / ("pagx-node-tool-" + std::to_string(id));
    ASSERT_TRUE(fs::create_directory(directory));
    root = directory;
    entry = root.filename().u8string() + ".js";
  }

  void TearDown() override {
    std::error_code ec;
    for (const auto& file : bundledFiles) fs::remove(file, ec);
    for (const auto& dir : bundledDirectories) fs::remove(dir, ec);
    if (!root.empty()) fs::remove_all(root, ec);
  }

  static std::string WriteFile(const fs::path& path, const std::string& contents = "") {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << contents;
    EXPECT_TRUE(file.good()) << path;
    return path.u8string();
  }

  std::string resolve() const {
    return pagx::cli::ResolveHTMLTool("PAGX_TEST_HTML_TOOL", entry.c_str(), entry.c_str());
  }

  std::string WriteBundled(const fs::path& parent) {
    auto dir = parent / "html-snapshot";
    if (fs::create_directory(dir)) bundledDirectories.push_back(dir);
    auto file = dir / entry;
    bundledFiles.push_back(file);
    return WriteFile(file);
  }

#ifndef _WIN32
  // Only the child may call the successful POSIX exec path: it replaces the test process.
  static int RunChild(const std::function<int()>& run) {
    auto pid = fork();
    if (pid == 0) _exit(run());
    EXPECT_GT(pid, 0);
    if (pid < 0) return -1;
    int status = 0;
    pid_t waited;
    do {
      waited = waitpid(pid, &status, 0);
    } while (waited < 0 && errno == EINTR);
    EXPECT_EQ(waited, pid);
    return status;
  }

  static std::vector<std::string> ReadArguments(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    EXPECT_TRUE(file.is_open());
    std::vector<std::string> args;
    std::string arg;
    while (std::getline(file, arg, '\0')) args.push_back(arg);
    return args;
  }

  // /bin/sh stands in for Node so native bridge tests need neither npm packages nor a browser.
  // NUL separators preserve empty arguments, embedded newlines, and shell metacharacters.
  std::string WriteRuntime(const std::string& body) {
    return WriteFile(root / fs::u8path("运行时 'quoted' $driver.sh"),
                     "if [ \"$#\" -gt 0 ]; then printf '%s\\0' \"$@\"; fi > "
                     "\"$PAGX_TEST_ARGUMENTS\"\n" +
                         body);
  }

  static std::string SnapshotOutput() {
    return "cat <<'HTML'\n<html><body data-pagx-capture-mode=\"current-state\" "
           "style=\"width:321px;height:123px\"><!--" +
           std::string(5000, 'x') +
           "--><div id=\"snapshot\" style=\"width:10px;height:20px;background:red\">"
           "</div></body></html>\nHTML\n";
  }
#endif

  fs::path root;
  std::string entry;
  std::vector<fs::path> bundledFiles;
  std::vector<fs::path> bundledDirectories;
};

TEST_F(PAGXNodeToolTest, CurrentExecutableIsCanonicalAbsoluteFile) {
  auto executable = fs::u8path(pagx::cli::CurrentExecutablePath());
  ASSERT_FALSE(executable.empty());
  EXPECT_TRUE(executable.is_absolute());
  EXPECT_TRUE(fs::is_regular_file(executable));
  EXPECT_EQ(executable, fs::canonical(executable));
}

TEST_F(PAGXNodeToolTest, NodeRuntimeDefaultsAndExplicitOverride) {
  ScopedEnvironment unset("PAGX_NODE_BIN", nullptr);
  EXPECT_EQ(pagx::cli::NodeExecutablePath(), "node");
  {
    ScopedEnvironment empty("PAGX_NODE_BIN", "");
    EXPECT_EQ(pagx::cli::NodeExecutablePath(), "node");
  }
  auto runtime = (root / fs::u8path("自定义 node")).u8string();
  ScopedEnvironment configured("PAGX_NODE_BIN", runtime.c_str());
  EXPECT_EQ(pagx::cli::NodeExecutablePath(), runtime);
}

TEST_F(PAGXNodeToolTest, ConfiguredToolOverridesCheckoutAndResolvesRelativeUnicodePath) {
  ScopedCurrentPath cwd(root);
  WriteFile(root / "tools/html-snapshot" / entry);
  const std::string relative = "脚本 'quoted'/driver.js";
  auto configuredPath = WriteFile(root / fs::u8path(relative));
  ScopedEnvironment configured("PAGX_TEST_HTML_TOOL", relative.c_str());
  EXPECT_EQ(resolve(), configuredPath);
}

TEST_F(PAGXNodeToolTest, InvalidConfiguredToolDoesNotFallBackToCheckout) {
  ScopedCurrentPath cwd(root);
  WriteFile(root / "tools/html-snapshot" / entry);
  for (const auto& path : {root, root / "missing.js"}) {
    SCOPED_TRACE(path.u8string());
    auto value = path.u8string();
    ScopedEnvironment configured("PAGX_TEST_HTML_TOOL", value.c_str());
    EXPECT_TRUE(resolve().empty());
  }
}

TEST_F(PAGXNodeToolTest, EmptyOverrideFindsRuntimeInCurrentDirectoryWithoutGit) {
  ScopedCurrentPath cwd(root);
  ScopedEnvironment configured("PAGX_TEST_HTML_TOOL", "");
  auto script = WriteFile(root / "tools/html-snapshot" / entry);
  EXPECT_EQ(resolve(), script);
}

TEST_F(PAGXNodeToolTest, BundledLayoutsTakePriorityOverCheckoutAndPreferNearestRuntime) {
  ScopedCurrentPath cwd(root);
  ScopedEnvironment configured("PAGX_TEST_HTML_TOOL", nullptr);
  WriteFile(root / "tools/html-snapshot" / entry);
  auto bin = fs::u8path(pagx::cli::CurrentExecutablePath()).parent_path();
  // npm's bin/<platform>-<arch>/pagx, a bin/pagx install, and a standalone pagx install.
  for (const auto& parent : {bin.parent_path().parent_path(), bin.parent_path(), bin}) {
    auto script = WriteBundled(parent);
    EXPECT_EQ(resolve(), script);
  }
  auto explicitScript = WriteFile(root / "override.js");
  ScopedEnvironment explicitOverride("PAGX_TEST_HTML_TOOL", explicitScript.c_str());
  EXPECT_EQ(resolve(), explicitScript);
}

TEST_F(PAGXNodeToolTest, AncestorCheckoutSupportsGitDirectoryAndWorktreeFile) {
  ScopedEnvironment configured("PAGX_TEST_HTML_TOOL", nullptr);
  auto script = WriteFile(root / "tools/html-snapshot" / entry);
  auto nested = root / "nested/work";
  fs::create_directories(nested);
  ScopedCurrentPath cwd(nested);
  fs::create_directory(root / ".git");
  EXPECT_EQ(resolve(), script);
  fs::remove(root / ".git");
  WriteFile(root / ".git", "gitdir: /unused/worktree\n");
  EXPECT_EQ(resolve(), script);
}

TEST_F(PAGXNodeToolTest, NearestGitRootStopsAncestorLookup) {
  ScopedEnvironment configured("PAGX_TEST_HTML_TOOL", nullptr);
  fs::create_directory(root / ".git");
  WriteFile(root / "tools/html-snapshot" / entry);
  auto nested = root / "nested";
  fs::create_directories(nested / ".git");
  fs::create_directories(nested / "work");
  ScopedCurrentPath cwd(nested / "work");
  EXPECT_TRUE(resolve().empty());
}

TEST_F(PAGXNodeToolTest, AncestorLookupHasBoundedDepth) {
  ScopedEnvironment configured("PAGX_TEST_HTML_TOOL", nullptr);
  fs::create_directory(root / ".git");
  auto script = WriteFile(root / "tools/html-snapshot" / entry);
  auto nested = root;
  for (int depth = 0; depth < 7; ++depth) nested /= "nested";
  fs::create_directories(nested / "too-deep");
  {
    ScopedCurrentPath cwd(nested);
    EXPECT_EQ(resolve(), script);
  }
  ScopedCurrentPath cwd(nested / "too-deep");
  EXPECT_TRUE(resolve().empty());
}

TEST_F(PAGXNodeToolTest, MissingRuntimeReturnsEmpty) {
  ScopedEnvironment configured("PAGX_TEST_HTML_TOOL", nullptr);
  ScopedCurrentPath cwd(root);
  EXPECT_TRUE(resolve().empty());
}

TEST_F(PAGXNodeToolTest, CheckoutNextToExecutableWorksFromUnrelatedDirectory) {
  auto checkout = fs::path(ProjectPath::Absolute(""));
  auto relative = fs::u8path(pagx::cli::CurrentExecutablePath()).lexically_relative(checkout);
  if (relative.empty() || *relative.begin() == "..") {
    GTEST_SKIP() << "Executable was built outside the source checkout";
  }
  ScopedEnvironment configured("PAGX_TEST_HTML_TOOL", nullptr);
  ScopedCurrentPath cwd(root);
  EXPECT_EQ(pagx::cli::ResolveHTMLTool("PAGX_TEST_HTML_TOOL", entry.c_str(), "snapshot.js"),
            (checkout / "tools/html-snapshot/snapshot.js").u8string());
}

TEST_F(PAGXNodeToolTest, ImportReportsMissingRuntime) {
  auto missing = (root / "missing.js").u8string();
  ScopedEnvironment configured("PAGX_IMPORT_BIN", missing.c_str());
  CaptureError error;
  EXPECT_EQ(CallRun(pagx::cli::RunImport, {"import", "page.html"}), 1);
  EXPECT_NE(error.str().find("pagx import: runtime not found"), std::string::npos);
  EXPECT_NE(error.str().find("PAGX_IMPORT_BIN"), std::string::npos);
}

TEST_F(PAGXNodeToolTest, ImportReportsNodeLaunchFailure) {
  auto script = WriteFile(root / "import.js");
  auto missingNode = (root / "missing-node").u8string();
  ScopedEnvironment configured("PAGX_IMPORT_BIN", script.c_str());
  ScopedEnvironment node("PAGX_NODE_BIN", missingNode.c_str());
  CaptureError error;
  EXPECT_EQ(CallRun(pagx::cli::RunImport, {"import", "page.html", "--output-dir", "space path"}),
            1);
  EXPECT_NE(error.str().find("pagx: failed to start Node"), std::string::npos);
  EXPECT_NE(error.str().find("PAGX_NODE_BIN"), std::string::npos);
}

#ifndef _WIN32
TEST_F(PAGXNodeToolTest, DeletedWorkingDirectoryReturnsEmpty) {
  ScopedEnvironment configured("PAGX_TEST_HTML_TOOL", nullptr);
  auto removed = root / "removed";
  fs::create_directory(removed);
  ScopedCurrentPath cwd(removed);
  ASSERT_TRUE(fs::remove(removed));
  EXPECT_TRUE(resolve().empty());
}

TEST_F(PAGXNodeToolTest, NodeLaunchFailureWithDefaultPathAndNonExecutableFile) {
  ScopedEnvironment path("PATH", "");
  ScopedEnvironment node("PAGX_NODE_BIN", nullptr);
  CaptureError error;
  EXPECT_EQ(pagx::cli::RunNodeTool("unused.js", {}), 1);
  EXPECT_NE(error.str().find("pagx: failed to start Node"), std::string::npos);
  auto denied = WriteFile(root / "not-executable");
  ScopedEnvironment overrideNode("PAGX_NODE_BIN", denied.c_str());
  EXPECT_EQ(pagx::cli::RunNodeTool("unused.js", {"--help"}), 1);
  EXPECT_NE(error.str().find(std::strerror(EACCES)), std::string::npos);
}

TEST_F(PAGXNodeToolTest, NodePreservesArgumentsAndNonzeroExitCode) {
  auto recorded = (root / "args").u8string();
  ScopedEnvironment output("PAGX_TEST_ARGUMENTS", recorded.c_str());
  ScopedEnvironment node("PAGX_NODE_BIN", "/bin/sh");
  auto script = WriteRuntime("exit 37\n");
  std::vector<std::string> args = {"",     "中文 空格",       "single' double\"",
                                   "a\nb", "$HOME; $(false)", "trailing\\"};
  auto status = RunChild([&] { return pagx::cli::RunNodeTool(script, args); });
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 37);
  EXPECT_EQ(ReadArguments(recorded), args);
}

TEST_F(PAGXNodeToolTest, NodeAcceptsEmptyArgumentsAndPreservesSignalTermination) {
  auto recorded = (root / "args").u8string();
  ScopedEnvironment output("PAGX_TEST_ARGUMENTS", recorded.c_str());
  ScopedEnvironment node("PAGX_NODE_BIN", "/bin/sh");
  auto script = WriteRuntime("kill -TERM \"$$\"\n");
  auto status = RunChild([&] { return pagx::cli::RunNodeTool(script, {}); });
  ASSERT_TRUE(WIFSIGNALED(status));
  EXPECT_EQ(WTERMSIG(status), SIGTERM);
  EXPECT_TRUE(ReadArguments(recorded).empty());
}

TEST_F(PAGXNodeToolTest, ImportPassesNormalizedRequestAndInvokingBinary) {
  auto recorded = (root / "args").u8string();
  ScopedEnvironment output("PAGX_TEST_ARGUMENTS", recorded.c_str());
  ScopedEnvironment node("PAGX_NODE_BIN", "/bin/sh");
  ScopedEnvironment unrelatedBinary("PAGX_BIN", "/unrelated/pagx");
  auto script = WriteRuntime("exit 23\n");
  ScopedEnvironment configured("PAGX_IMPORT_BIN", script.c_str());
  std::vector<std::string> args = {"import",           "页面 'one'.html", "--html-scenario",
                                   "states file.json", "--output-dir",    "输出 folder",
                                   "--preview",        "--images",        "embed"};
  auto status = RunChild([&] { return CallRun(pagx::cli::RunImport, args); });
  ASSERT_TRUE(WIFEXITED(status));
  EXPECT_EQ(WEXITSTATUS(status), 23);
  auto forwarded = ReadArguments(recorded);
  ASSERT_EQ(forwarded.size(), 2u);
  EXPECT_EQ(forwarded[0], "--request");
  EXPECT_NE(forwarded[1].find("页面 'one'.html"), std::string::npos);
  EXPECT_NE(forwarded[1].find(pagx::cli::CurrentExecutablePath()), std::string::npos);
  EXPECT_EQ(forwarded[1].find("/unrelated/pagx"), std::string::npos);
  EXPECT_NE(forwarded[1].find("\"pagxImages\":\"embed\""), std::string::npos);
  EXPECT_NE(forwarded[1].find("\"doRender\":true"), std::string::npos);
}

TEST_F(PAGXNodeToolTest, InvalidImportArgumentsNeverStartNode) {
  ScopedEnvironment node("PAGX_NODE_BIN", "/missing-node");
  for (const auto& args : std::vector<std::vector<std::string>>{
           {"import"},
           {"import", "page.html", "--scenario", "states.json"},
           {"import", "page.html", "--html-checkpoint", "ready"},
           {"import", "page.html", "--preview", "--no-resolve"},
           {"import", "page.svg", "--html-browser-engine", "puppeteer"}}) {
    CaptureError error;
    EXPECT_EQ(CallRun(pagx::cli::RunImport, args), 2);
    EXPECT_EQ(error.str().find("start Node"), std::string::npos);
  }
}

TEST_F(PAGXNodeToolTest, SingleDocumentSnapshotPreservesQuotedArguments) {
  auto recorded = (root / "args").u8string();
  ScopedEnvironment output("PAGX_TEST_ARGUMENTS", recorded.c_str());
  ScopedEnvironment node("PAGX_NODE_BIN", "/bin/sh");
  auto script = WriteRuntime(SnapshotOutput());
  ScopedEnvironment configured("PAGX_HTML_SNAPSHOT_BIN", script.c_str());
  const std::string input = "HTTPS://example.invalid/page?a=1&b='two'";
  auto result = pagx::cli::ImportFile(input, "", {});
  ASSERT_NE(result.document, nullptr) << result.error;
  EXPECT_EQ(ReadArguments(recorded), (std::vector<std::string>{input, "-o", "-"}));
}

TEST_F(PAGXNodeToolTest, ImportRejectsEmptyOutputAndPropagatesSnapshotFailure) {
  auto recorded = (root / "args").u8string();
  ScopedEnvironment output("PAGX_TEST_ARGUMENTS", recorded.c_str());
  ScopedEnvironment node("PAGX_NODE_BIN", "/bin/sh");
  for (int exitCode : {0, 19, 127}) {
    SCOPED_TRACE(exitCode);
    auto script = WriteRuntime("exit " + std::to_string(exitCode) + "\n");
    ScopedEnvironment configured("PAGX_HTML_SNAPSHOT_BIN", script.c_str());
    auto result = pagx::cli::ImportFile("page.html", "html", {});
    EXPECT_EQ(result.document, nullptr);
    auto expected = exitCode == 0     ? "produced empty output"
                    : exitCode == 127 ? "`node` not found on PATH"
                                      : "failed (exit code 19)";
    EXPECT_NE(result.error.find(expected), std::string::npos) << result.error;
  }
}
#endif

}  // namespace
}  // namespace pag
