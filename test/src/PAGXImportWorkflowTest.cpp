// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include "base/PAGTest.h"
#include "cli/CommandImport.h"
#include "cli/ImportRuntime.h"
#include "cli/ImportSource.h"
#include "cli/NodeTool.h"
#include "pagx/nodes/Image.h"
#include "utils/PAGXTestUtils.h"
#include "utils/TestDir.h"

namespace pag {
namespace fs = std::filesystem;
using namespace pagx::cli;
using JSON = nlohmann::json;
namespace {
class ImportTestEnvironment {
 public:
  ImportTestEnvironment(const char* name, const std::string& value) : name(name) {
#ifdef _WIN32
    const wchar_t* previous = _wgetenv(UTF8ToWide(name).c_str());
    present = previous != nullptr;
    if (previous) saved = WideToUTF8(previous);
#else
    const char* previous = std::getenv(name);
    present = previous != nullptr;
    if (previous) saved = previous;
#endif
    set(value.c_str());
  }
  ~ImportTestEnvironment() {
    set(present ? saved.c_str() : nullptr);
  }

 private:
  void set(const char* value) {
#ifdef _WIN32
    _wputenv_s(UTF8ToWide(name).c_str(), UTF8ToWide(value ? value : "").c_str());
#else
    if (value) setenv(name, value, 1);
    else
      unsetenv(name);
#endif
  }
  const char* name;
  bool present;
  std::string saved;
};

std::string ReadImportTestFile(const fs::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
const char* TestSVG =
    "<svg width='32' height='24'><rect width='32' height='24' fill='#ff0000'/></svg>";

class ImportTestWorkingDirectory {
 public:
  explicit ImportTestWorkingDirectory(const fs::path& path) : previous(fs::current_path()) {
    fs::current_path(path);
  }
  ~ImportTestWorkingDirectory() {
    fs::current_path(previous);
  }

 private:
  fs::path previous;
};
}  // namespace

CLI_TEST(PAGXImportRuntimeTest, TemporaryDirectoriesCleanUpOnException) {
  ImportTempDirectory parent;
  fs::path first, second;
  try {
    ImportTempDirectory a(parent.path), b(parent.path);
    first = a.path;
    second = b.path;
    EXPECT_NE(first, second);
    WriteImportFile(first / "nested/binary", std::string("a\0b", 3));
    EXPECT_EQ(ReadImportTestFile(first / "nested/binary"), std::string("a\0b", 3));
    throw std::runtime_error("unwind");
  } catch (const std::runtime_error&) {
  }
  EXPECT_FALSE(fs::exists(first));
  EXPECT_FALSE(fs::exists(second));
  EXPECT_TRUE(fs::is_empty(parent.path));
}

CLI_TEST(PAGXImportRuntimeTest, FailedPublicationKeepsSourceAndDestination) {
  ImportTempDirectory temp;
  auto source = temp.path / "source", target = temp.path / "target";
  WriteImportFile(source, "new");
  WriteImportFile(target, "old");
  EXPECT_THROW(PublishImportFile(source, target, false), std::exception);
  EXPECT_EQ(ReadImportTestFile(source), "new");
  EXPECT_EQ(ReadImportTestFile(target), "old");
  EXPECT_NO_THROW(PublishImportFile(source, target, true));
  EXPECT_FALSE(fs::exists(source));
  EXPECT_EQ(ReadImportTestFile(target), "new");
  EXPECT_THROW(WriteImportFile(target / "child", "bad"), std::exception);
  EXPECT_EQ(ReadImportTestFile(target), "new");
}

CLI_TEST(PAGXImportRuntimeTest, JSONErrorsAndRuntimeOverridesAreExplicit) {
  ImportTempDirectory temp;
  auto file = temp.path / "protocol.json";
  EXPECT_THROW(ReadImportJSON(file), std::runtime_error);
  WriteImportFile(file, "{invalid");
  EXPECT_THROW(ReadImportJSON(file), JSON::parse_error);
  WriteImportFile(file, "{\"version\":1,\"text\":\"中文\"}");
  EXPECT_EQ(ReadImportJSON(file).at("text"), "中文");
  EXPECT_TRUE(fs::is_regular_file(fs::u8path(CurrentExecutablePath())));
  ImportTestEnvironment node("PAGX_NODE_BIN", "");
  EXPECT_EQ(NodeExecutablePath(), "node");
  ImportTestEnvironment overrideNode("PAGX_NODE_BIN", "custom node");
  EXPECT_EQ(NodeExecutablePath(), "custom node");
  ImportTestEnvironment tool("PAGX_HTML_IMPORT_BIN", file.u8string());
  EXPECT_EQ(ResolveHTMLTool("PAGX_HTML_IMPORT_BIN", "launch-import.js", "import-adapter.js"),
            file.u8string());
  ImportTestEnvironment missing("PAGX_HTML_IMPORT_BIN", (temp.path / "missing").u8string());
  ImportAdapter adapter;
  JSON response;
  EXPECT_THROW(adapter.run({{"operation", "inspect"}}, &response), std::runtime_error);
}

CLI_TEST(PAGXImportRuntimeTest, CancellationPreventsSpawningAndScopeRestoresSignalHandlers) {
  for (const auto signal : {SIGINT, SIGTERM}) {
    auto previous = std::signal(signal, SIG_IGN);
    {
      ToolProcessScope scope;
      EXPECT_EQ(ToolCancellationCode(), 0);
      std::raise(signal);
      EXPECT_EQ(ToolCancellationCode(), signal == SIGINT ? 130 : 143);
      EXPECT_EQ(RunNodeToolWait("unused", {}), signal == SIGINT ? 130 : 143);
    }
    EXPECT_EQ(ToolCancellationCode(), 0);
    EXPECT_EQ(std::signal(signal, previous), SIG_IGN);
  }
}

CLI_TEST(PAGXImportRuntimeTest, RuntimeDiscoveryFindsCheckoutAndRejectsMissingEntrypoints) {
  ImportTempDirectory temp;
  ImportTestEnvironment environment("PAGX_HTML_IMPORT_BIN", "");
  WriteImportFile(temp.path / "tools/html-snapshot/adapter.js", "// test adapter");
  fs::create_directory(temp.path / ".git");
  fs::create_directories(temp.path / "nested/work");
  ImportTestWorkingDirectory cwd(temp.path);
  auto expected = (fs::canonical(temp.path) / "tools/html-snapshot/adapter.js").u8string();
  EXPECT_EQ(ResolveHTMLTool("PAGX_HTML_IMPORT_BIN", "no-such-bundle.js", "adapter.js"), expected);
  fs::current_path(temp.path / "nested/work");
  EXPECT_EQ(ResolveHTMLTool("PAGX_HTML_IMPORT_BIN", "no-such-bundle.js", "adapter.js"), expected);
  EXPECT_TRUE(
      ResolveHTMLTool("PAGX_HTML_IMPORT_BIN", "no-such-bundle.js", "no-such-source.js").empty());
}

CLI_TEST(PAGXImportRuntimeTest, PreparedSourceReportsMissingInvalidAndUnreadableFonts) {
  ImportTempDirectory temp;
  auto file = temp.path / "page.html";
  ImportItem item;
  item.input = file.u8string();
  item.format = "html";
  ImportRequest options;
  auto result = ImportPreparedSource(item, item.input, "", options);
  EXPECT_NE(result.error.find("cannot read prepared source"), std::string::npos);
  WriteImportFile(file, "");
  result = ImportPreparedSource(item, item.input, "", options);
  EXPECT_NE(result.error.find("failed to parse prepared HTML"), std::string::npos);
  WriteImportFile(file, "<html><body style='width:32px;height:24px'>text</body></html>");
  auto missingFont = (temp.path / "missing.ttf").u8string();
  result = ImportPreparedSource(item, item.input, "", options, {missingFont});
  EXPECT_NE(result.error.find("cannot load the requested fonts"), std::string::npos);
}

CLI_TEST(PAGXImportRuntimeTest, PreparedSourceDoesNotAcceptEmptyOrRemoteImageBytes) {
  ImportTempDirectory temp;
  WriteImportFile(temp.path / "empty.png", "");
  ImportItem item;
  item.input = (temp.path / "page.svg").u8string();
  item.format = "svg";
  ImportRequest options;
  options.resolve = false;
  for (const auto& image :
       {"empty.png", "https://example.test/missing.png", "data:image/png;base64,"}) {
    SCOPED_TRACE(image);
    WriteImportFile(fs::u8path(item.input),
                    std::string("<svg width='32' height='24'><image href='") + image +
                        "' width='32' height='24'/></svg>");
    auto result = ImportPreparedSource(item, item.input, temp.path.u8string(), options);
    EXPECT_NE(result.error.find("cannot load image resource"), std::string::npos);
  }
}

CLI_TEST(PAGXImportRuntimeTest, ExternalImagesKeepFormatAndBytesAndEmbedAvoidsSidecars) {
  ImportTempDirectory temp;
  auto resources = fs::path(TestDir::GetRoot()).parent_path() / "resources/apitest";
  auto input = temp.path / "images.svg";
  std::string svg = "<svg width='32' height='24'>";
  for (const auto& extension : {"png", "jpg", "webp"}) {
    auto name = std::string("imageReplacement.") + extension;
    fs::copy_file(resources / name, temp.path / name);
    svg += "<image href='" + name + "' width='8' height='8'/>";
  }
  WriteImportFile(temp.path / "opaque.bin", "opaque image bytes");
  svg += "<image href='opaque.bin' width='8' height='8'/></svg>";
  WriteImportFile(input, svg);
  auto output = temp.path / "out";
  ASSERT_EQ(CallRun(RunImport, {"import", input.u8string(), "--output-dir", output.u8string()}), 0);
  auto data = ReadImportJSON(output / "manifest.json");
  auto assets = data["outputs"][0]["assets"];
  ASSERT_EQ(assets.size(), 4u);
  std::set<std::string> extensions;
  for (const auto& asset : assets) {
    auto path = output / fs::u8path(asset.get<std::string>());
    auto extension = path.extension().u8string();
    extensions.insert(extension);
    auto original = extension == ".bin" ? temp.path / "opaque.bin"
                                        : resources / ("imageReplacement" + extension);
    EXPECT_EQ(ReadImportTestFile(path), ReadImportTestFile(original));
    EXPECT_NE(ReadImportTestFile(output / "images.pagx").find(asset.get<std::string>()),
              std::string::npos);
  }
  EXPECT_EQ(extensions, (std::set<std::string>{".png", ".jpg", ".webp", ".bin"}));
  auto embedded = temp.path / "embedded.pagx";
  ASSERT_EQ(CallRun(RunImport,
                    {"import", input.u8string(), "-o", embedded.u8string(), "--images", "embed"}),
            0);
  EXPECT_FALSE(fs::exists(temp.path / "embedded.assets"));
  EXPECT_NE(ReadImportTestFile(embedded).find("base64,"), std::string::npos);
}

// A real Node child implements the protocol boundary. Native conversion, filesystem
// publication, JSON validation, exit handling and manifest updates remain production code.
class PAGXImportWorkflowTest : public testing::Test {
 protected:
  ImportTempDirectory temp;
  fs::path script = temp.path / "adapter 中文 ' &.js";
  fs::path input = temp.path / "page.html";
  fs::path output = temp.path / "results";
  std::unique_ptr<ImportTestEnvironment> environment;

  void SetUp() override {
    if (RunNodeToolWait("-e", {"process.exit(0)"}) != 0) GTEST_SKIP() << "Node.js is unavailable";
    environment =
        std::make_unique<ImportTestEnvironment>("PAGX_HTML_IMPORT_BIN", script.u8string());
    WriteImportFile(input, "<html><body style='width:32px;height:24px;background:red'/></html>");
  }
  void adapter(const std::string& body) {
    WriteImportFile(script, R"JS(
const fs = require('fs'), path = require('path');
const [requestFile, responseFile] = process.argv.slice(2);
const req = JSON.parse(fs.readFileSync(requestFile, 'utf8'));
const reply = value => fs.writeFileSync(responseFile, JSON.stringify({version:1, ...value}));
const captured = (id = '0', checkpoint = '') => ({id, checkpoint, snapshot:req.inputs[0].input,
  url:'https://example.test/final', warnings:['captured warning'], fonts:[]});
)JS" + body);
  }
  int run(std::vector<std::string> extra = {}) {
    std::vector<std::string> args = {"import", input.u8string(), "--output-dir", output.u8string()};
    args.insert(args.end(), extra.begin(), extra.end());
    return CallRun(RunImport, args);
  }
  JSON manifest() {
    return ReadImportJSON(output / "manifest.json");
  }
};

TEST_F(PAGXImportWorkflowTest, AdapterVersionsRequestsAndClearsStaleResponses) {
  adapter(R"JS(
if (req.mode === 'exit') process.exit(17);
reply({status:'complete', request:req});
)JS");
  ImportAdapter bridge;
  JSON response;
  ASSERT_EQ(bridge.run({{"operation", "capture"}, {"version", 99}}, &response), 0);
  EXPECT_EQ(response["request"]["version"], 1);
  auto work = fs::u8path(response["request"]["workDir"].get<std::string>());
  EXPECT_TRUE(work.is_absolute());
  EXPECT_TRUE(fs::is_directory(work));
  EXPECT_EQ(bridge.run({{"operation", "inspect"}, {"mode", "exit"}}, &response), 17);
  EXPECT_TRUE(response.is_null());
}

TEST_F(PAGXImportWorkflowTest, AdapterRejectsMissingMalformedAndWrongVersionResponses) {
  for (const auto& body :
       {"", "fs.writeFileSync(responseFile, '{bad');", "fs.writeFileSync(responseFile, '[]');",
        "reply({version:2});", "reply({version:'1'});"}) {
    SCOPED_TRACE(body);
    adapter(body);
    ImportAdapter bridge;
    JSON response = {{"stale", true}};
    EXPECT_THROW(bridge.run({{"operation", "inspect"}}, &response), std::exception);
  }
}

TEST_F(PAGXImportWorkflowTest, NodeReceivesLiteralArgumentsAndPreservesExitStatus) {
  auto result = temp.path / "argv.json";
  WriteImportFile(script,
                  "require('fs').writeFileSync(process.argv[2], "
                  "JSON.stringify(process.argv.slice(3))); process.exit(23);");
  std::vector<std::string> values = {
      "", "with spaces", "中文", "'\"&;|", "$(touch unwanted)", "trailing\\", "line\nbreak"};
  auto args = values;
  args.insert(args.begin(), result.u8string());
  EXPECT_EQ(RunNodeToolWait(script.u8string(), args), 23);
  EXPECT_EQ(ReadImportJSON(result), JSON(values));
  ImportTestEnvironment absent("PAGX_NODE_BIN", (temp.path / "no-node").u8string());
#ifdef _WIN32
  EXPECT_EQ(RunNodeToolWait(script.u8string(), {}), 1);
#else
  EXPECT_EQ(RunNodeToolWait(script.u8string(), {}), 127);
  EXPECT_EQ(RunNodeTool(script.u8string(), {}), 1);
#endif
}

#ifndef _WIN32
TEST_F(PAGXImportWorkflowTest, CancellationSignalsReachTheChildAndManifest) {
  auto marker = temp.path / "closed";
  WriteImportFile(script, R"JS(
const fs = require('fs');
process.on('SIGTERM', () => { fs.writeFileSync(process.argv[2], 'closed'); setTimeout(() => process.exit(0), 80); });
process.kill(process.ppid, 'SIGTERM');
setInterval(() => {}, 1000);
)JS");
  {
    ToolProcessScope scope;
    EXPECT_EQ(RunNodeToolWait(script.u8string(), {marker.u8string()}), 143);
    EXPECT_EQ(ReadImportTestFile(marker), "closed");
  }
  EXPECT_EQ(ToolCancellationCode(), 0);
  adapter(
      "if(req.operation === 'inspect') reply({checkpoints:[]}); "
      "else { process.kill(process.ppid, 'SIGINT'); setInterval(() => {}, 1000); }");
  EXPECT_EQ(run({"--preview"}), 130);
  auto data = manifest();
  EXPECT_EQ(data["status"], "cancelled");
  EXPECT_EQ(data["outputs"][0]["status"], "cancelled");
  EXPECT_EQ(data["preview"]["status"], "cancelled");
  EXPECT_FALSE(fs::exists(output / "page.pagx"));
}
#endif

TEST_F(PAGXImportWorkflowTest, DownloadedFontsArePublishedByteForByte) {
  auto font =
      fs::path(TestDir::GetRoot()).parent_path() / "resources/font/NotoSansHebrew-Regular.ttf";
  ASSERT_TRUE(fs::is_regular_file(font));
  adapter(
      "if(req.operation === 'inspect') reply({checkpoints:[]}); else { "
      "const out = captured(); out.fonts = [" +
      JSON(font.u8string()).dump() + "]; reply({outputs:[out]}); }");
  ASSERT_EQ(run({"--download-fonts"}), 0);
  auto fonts = manifest()["outputs"][0]["fonts"];
  ASSERT_EQ(fonts.size(), 1u);
  EXPECT_EQ(fonts[0], "page.fonts/1-NotoSansHebrew-Regular.ttf");
  EXPECT_EQ(ReadImportTestFile(output / fs::u8path(fonts[0].get<std::string>())),
            ReadImportTestFile(font));
}

TEST_F(PAGXImportWorkflowTest, LibrarySnapshotForwardsLiteralScenarioAndAnimationArguments) {
  ImportTestEnvironment enabled("PAGX_HTML_SNAPSHOT", "1");
  ImportTestEnvironment snapshot("PAGX_HTML_SNAPSHOT_BIN", script.u8string());
  auto arguments = temp.path / "snapshot-args.json";
  WriteImportFile(script, "require('fs').writeFileSync(" + JSON(arguments.u8string()).dump() +
                              ", JSON.stringify(process.argv.slice(2))); process.stdout.write(" +
                              JSON(ReadImportTestFile(input)).dump() + ");");
  ImportFormatOptions options;
  options.scenario = (temp.path / "states 中文 ' &.json").u8string();
  options.checkpoint = "state ' & $(echo literal)";
  auto imported = ImportFile(input.u8string(), "html", options);
  ASSERT_TRUE(imported.error.empty()) << imported.error;
  ASSERT_NE(imported.document, nullptr);
  EXPECT_EQ(imported.document->width, 32);
  EXPECT_EQ(ReadImportJSON(arguments),
            (JSON::array({input.u8string(), "-o", "-", "--scenario", options.scenario,
                          "--checkpoint", options.checkpoint})));
  options = {};
  options.captureAnimations = true;
  imported = ImportFile(input.u8string(), "html", options);
  ASSERT_TRUE(imported.error.empty()) << imported.error;
  EXPECT_EQ(ReadImportJSON(arguments),
            (JSON::array({input.u8string(), "-o", "-", "--capture-animations"})));
}

TEST_F(PAGXImportWorkflowTest, LibrarySnapshotFailuresDoNotReturnPartialDocuments) {
  ImportTestEnvironment enabled("PAGX_HTML_SNAPSHOT", "1");
  ImportTestEnvironment snapshot("PAGX_HTML_SNAPSHOT_BIN", script.u8string());
  for (const auto& test : std::vector<std::pair<std::string, std::string>>{
           {"", "produced empty output"},
           {"process.stdout.write('<html/>'); process.exit(17);", "exit code 17"}}) {
    WriteImportFile(script, test.first);
    auto result = ImportFile(input.u8string(), "html", {});
    EXPECT_EQ(result.document, nullptr);
    EXPECT_NE(result.error.find(test.second), std::string::npos) << result.error;
  }
}

CLI_TEST(PAGXImportRuntimeTest, LibraryScenarioOptionsAreValidatedBeforeRunningSnapshot) {
  ImportFormatOptions options;
  options.checkpoint = "one";
  auto result = ImportFile("missing.html", "html", options);
  EXPECT_NE(result.error.find("--checkpoint requires --scenario"), std::string::npos);
  options.scenario = "states.json";
  {
    ImportTestEnvironment enabled("PAGX_HTML_SNAPSHOT", "1");
    result = ImportFile("missing.svg", "svg", options);
    EXPECT_NE(result.error.find("--scenario requires HTML"), std::string::npos);
    options.captureAnimations = true;
    result = ImportFile("missing.html", "html", options);
    EXPECT_NE(result.error.find("cannot be combined"), std::string::npos);
    options.captureAnimations = false;
  }
  ImportTestEnvironment disabled("PAGX_HTML_SNAPSHOT", "0");
  result = ImportFile("missing.html", "html", options);
  EXPECT_NE(result.error.find("pre-pass enabled"), std::string::npos);
}

TEST_F(PAGXImportWorkflowTest, BrowserOptionsAndCapturedMetadataReachTheManifest) {
  adapter(R"JS(
fs.writeFileSync(path.join(path.dirname(__filename), req.operation + '.json'), JSON.stringify(req));
reply(req.operation === 'inspect' ? {checkpoints:[]} : {outputs:[captured()], errors:[]});
)JS");
  ASSERT_EQ(run({"--viewport-width", "640", "--viewport-height", "480", "--wait-ms", "0",
                 "--selector", "#ready", "--keep-snapshot", "--verbose", "--images", "embed"}),
            0);
  auto request = ReadImportJSON(temp.path / "capture.json");
  EXPECT_EQ(request["options"]["viewportWidth"], 640);
  EXPECT_EQ(request["options"]["viewportHeight"], 480);
  EXPECT_EQ(request["options"]["waitMs"], 0);
  EXPECT_EQ(request["options"]["selector"], "#ready");
  auto data = manifest();
  EXPECT_EQ(data["status"], "complete");
  ASSERT_EQ(data["outputs"].size(), 1u);
  auto entry = data["outputs"][0];
  EXPECT_EQ(entry["status"], "complete");
  EXPECT_EQ(entry["url"], "https://example.test/final");
  EXPECT_EQ(entry["warnings"][0], "captured warning");
  EXPECT_EQ(entry["width"], 32);
  EXPECT_EQ(entry["height"], 24);
  EXPECT_EQ(entry["snapshot"], "page.subset.html");
  EXPECT_EQ(ReadImportTestFile(output / "page.subset.html"), ReadImportTestFile(input));
  VerifyFile((output / "page.pagx").u8string(), "imported browser snapshot");
}

TEST_F(PAGXImportWorkflowTest, DryRunInspectsButDoesNotCaptureOrCreateOutputs) {
  adapter(
      "if(req.operation !== 'inspect') process.exit(31); reply({checkpoints:['later','first']});");
  testing::internal::CaptureStdout();
  auto code = run({"--scenario", "states.json", "--preview", "--keep-snapshot", "--dry-run"});
  auto text = testing::internal::GetCapturedStdout();
  ASSERT_EQ(code, 0) << text;
  auto data = JSON::parse(text);
  EXPECT_EQ(data["status"], "planned");
  EXPECT_EQ(data["outputs"][0]["checkpoint"], "later");
  EXPECT_EQ(data["outputs"][1]["checkpoint"], "first");
  EXPECT_EQ(data["outputs"][0]["png"], "page.later.png");
  EXPECT_EQ(data["preview"]["status"], "pending");
  EXPECT_FALSE(fs::exists(output));
  adapter("process.exit(12);");
  EXPECT_EQ(run(), 2);
  EXPECT_FALSE(fs::exists(output));
  EXPECT_EQ(run({"--scenario", "states.json"}), 2);
}

TEST_F(PAGXImportWorkflowTest, PartialCapturesKeepCompletedArtifactsAndReportEveryFailure) {
  adapter(R"JS(
if (req.operation === 'inspect') reply({checkpoints:['first','missing']});
else { reply({outputs:[captured('0','first')], errors:[{id:'0',error:'interaction failed'}]}); process.exitCode=1; }
)JS");
  EXPECT_EQ(run({"--scenario", "states.json"}), 1);
  auto data = manifest();
  EXPECT_EQ(data["status"], "failed");
  EXPECT_EQ(data["outputs"][0]["status"], "complete");
  EXPECT_EQ(data["outputs"][1]["status"], "failed");
  EXPECT_EQ(data["errors"].size(), 2u);
  EXPECT_TRUE(fs::is_regular_file(output / "page.first.pagx"));
  EXPECT_FALSE(fs::exists(output / "page.missing.pagx"));
}

TEST_F(PAGXImportWorkflowTest, DuplicateAndMissingCaptureResultsFailTheManifest) {
  for (const auto& body : {"reply({outputs:[captured(),captured()]});", "process.exit(19);"}) {
    SCOPED_TRACE(body);
    adapter("if(req.operation === 'inspect') reply({checkpoints:[]}); else {" + std::string(body) +
            "}");
    EXPECT_EQ(run({"--overwrite"}), 1);
    auto data = manifest();
    EXPECT_EQ(data["status"], "failed");
    EXPECT_EQ(data["outputs"][0]["status"], "failed");
    EXPECT_FALSE(data["errors"].empty());
    EXPECT_FALSE(fs::exists(output / "page.pagx"));
  }
}

TEST_F(PAGXImportWorkflowTest, PreviewRequiresEveryOutputAndRejectsEscapedPaths) {
  adapter(R"JS(
if(req.operation === 'inspect') reply({checkpoints:[]});
else if(req.operation === 'capture') reply({outputs:[], errors:[]});
else throw Error('preview must not run');
)JS");
  EXPECT_EQ(run({"--preview"}), 1);
  EXPECT_EQ(manifest()["preview"]["status"], "failed");
  EXPECT_FALSE(fs::exists(output / "preview"));
  // Prepared SVG avoids browser acquisition but still exercises real native PNG rendering.
  input = temp.path / "icon.svg";
  WriteImportFile(input, TestSVG);
  for (const auto& response : {"reply({status:'failed'});", "reply({status:'complete',pages:[]});",
                               "reply({status:'complete',pages:[{file:__filename}]});"}) {
    adapter(response);
    EXPECT_EQ(run({"--preview", "--overwrite"}), 1);
    auto data = manifest();
    EXPECT_EQ(data["outputs"][0]["status"], "complete");
    EXPECT_EQ(data["preview"]["status"], "failed");
    EXPECT_TRUE(fs::is_regular_file(output / "icon.png"));
    EXPECT_FALSE(fs::exists(output / "preview" / script.filename()));
  }
}

TEST_F(PAGXImportWorkflowTest, PublishesPreviewAndPreservesUnrelatedFilesOnOverwrite) {
  input = temp.path / "icon.svg";
  WriteImportFile(input, TestSVG);
  adapter(R"JS(
if(req.preview.columns !== 2 || req.preview.items[0].name !== 'icon.svg') process.exit(15);
fs.mkdirSync(req.preview.outputDir, {recursive:true});
const file = path.join(req.preview.outputDir, 'sheet.png');
fs.copyFileSync(req.preview.items[0].png, file);
reply({status:'complete',pages:[{file,items:['icon.svg']}]});
)JS");
  ASSERT_EQ(run({"--preview", "--preview-columns", "2", "--scale", "2"}), 0);
  auto data = manifest();
  EXPECT_EQ(data["preview"]["status"], "complete");
  EXPECT_EQ(data["preview"]["pages"][0]["file"], "preview/sheet.png");
  EXPECT_EQ(ReadImportTestFile(output / "preview/sheet.png"),
            ReadImportTestFile(output / "icon.png"));
  WriteImportFile(output / "keep.txt", "unrelated");
  EXPECT_EQ(run(), 2);
  ASSERT_EQ(run({"--overwrite"}), 0);
  EXPECT_EQ(ReadImportTestFile(output / "keep.txt"), "unrelated");
  EXPECT_TRUE(fs::exists(output / "preview/sheet.png"));
  EXPECT_FALSE(manifest().contains("preview"));
}

TEST_F(PAGXImportWorkflowTest, PreviewLabelsPreserveBatchPathsAndCheckpointOrder) {
  adapter(R"JS(
if(req.operation === 'inspect') reply({checkpoints:['second','first']});
else if(req.operation === 'capture') reply({outputs:[captured('0','second'),captured('0','first')]});
else {
  fs.mkdirSync(req.preview.outputDir, {recursive:true});
  const file = path.join(req.preview.outputDir, 'sheet.png');
  fs.copyFileSync(req.preview.items[0].png, file);
  reply({status:'complete',pages:[{file,items:req.preview.items.map(item=>item.name)}]});
}
)JS");
  auto batch = temp.path / "batch";
  WriteImportFile(batch / "nested/icon.svg", TestSVG);
  ASSERT_EQ(CallRun(RunImport, {"import", "--batch", batch.u8string(), "--output-dir",
                                output.u8string(), "--preview"}),
            0);
  EXPECT_EQ(manifest()["preview"]["pages"][0]["items"], JSON::array({"nested/icon.svg"}));
  output = temp.path / "states";
  ASSERT_EQ(run({"--scenario", "states.json", "--preview"}), 0);
  auto data = manifest();
  EXPECT_EQ(data["outputs"][0]["checkpoint"], "second");
  EXPECT_EQ(data["outputs"][1]["checkpoint"], "first");
  EXPECT_EQ(data["preview"]["pages"][0]["items"], JSON::array({"second", "first"}));
}

CLI_TEST(PAGXImportRuntimeTest, BatchConversionContinuesAfterAnInvalidSource) {
  ImportTempDirectory temp;
  auto inputs = temp.path / "inputs", outputs = temp.path / "outputs";
  fs::create_directory(inputs);
  WriteImportFile(inputs / "a.svg", "not SVG");
  WriteImportFile(inputs / "b.svg", TestSVG);
  EXPECT_EQ(CallRun(RunImport, {"import", "--batch", inputs.u8string(), "--output-dir",
                                outputs.u8string(), "--images", "embed"}),
            1);
  auto data = ReadImportJSON(outputs / "manifest.json");
  EXPECT_EQ(data["status"], "failed");
  EXPECT_EQ(data["outputs"][0]["status"], "failed");
  EXPECT_EQ(data["outputs"][1]["status"], "complete");
  EXPECT_FALSE(fs::exists(outputs / "a.pagx"));
  VerifyFile((outputs / "b.pagx").u8string(), "batch survives malformed SVG");
}
}  // namespace pag
