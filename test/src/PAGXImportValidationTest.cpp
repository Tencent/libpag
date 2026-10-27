// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#include <filesystem>
#include <fstream>
#include "base/PAGTest.h"
#include "cli/ImportPlan.h"
#include "cli/ImportRuntime.h"

namespace pag {
namespace fs = std::filesystem;
using namespace pagx::cli;

CLI_TEST(PAGXImportValidationTest, ParsesAllBrowserOptionsAndRepeatedCredentials) {
  ImportRequest options;
  bool help = true;
  std::string error = "previous failure";
  ASSERT_TRUE(ParseImportRequest({"--input",
                                  "https://example.test/page",
                                  "--output-dir",
                                  "out",
                                  "--output-name",
                                  "page",
                                  "--html-browser-engine",
                                  "playwright",
                                  "--html-capture-animations",
                                  "--html-scroll-reveal",
                                  "--html-no-inline-icon-fonts",
                                  "--html-download-fonts",
                                  "--html-keep-snapshot",
                                  "--embed-fonts",
                                  "--html-cookie",
                                  "a=1",
                                  "--html-cookie",
                                  "b=two=three",
                                  "--html-header",
                                  "X-A: 1",
                                  "--html-header",
                                  "X-B: 2",
                                  "--html-viewport-width",
                                  "16384",
                                  "--html-viewport-height",
                                  "1",
                                  "--html-wait-ms",
                                  "0",
                                  "--html-selector",
                                  "#ready",
                                  "--images",
                                  "embed",
                                  "--preview",
                                  "--preview-columns",
                                  "32",
                                  "--scale",
                                  "0.5",
                                  "--overwrite",
                                  "--dry-run",
                                  "-v"},
                                 &options, &help, &error))
      << error;
  EXPECT_FALSE(help);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(options.input, "https://example.test/page");
  EXPECT_EQ(options.browserEngine, "playwright");
  EXPECT_TRUE(options.captureAnimations);
  EXPECT_TRUE(options.scrollReveal);
  EXPECT_FALSE(options.inlineIconFonts);
  EXPECT_TRUE(options.downloadFonts);
  EXPECT_TRUE(options.keepSnapshot);
  EXPECT_TRUE(options.embedFonts);
  EXPECT_EQ(options.cookies, (std::vector<std::string>{"a=1", "b=two=three"}));
  EXPECT_EQ(options.headers, (std::vector<std::string>{"X-A: 1", "X-B: 2"}));
  EXPECT_EQ(options.browserOptions.at("viewport-width"), "16384");
  EXPECT_EQ(options.browserOptions.at("viewport-height"), "1");
  EXPECT_EQ(options.browserOptions.at("wait-ms"), "0");
  EXPECT_EQ(options.browserOptions.at("selector"), "#ready");
  EXPECT_EQ(options.images, ImageStorageMode::Embed);
  EXPECT_TRUE(options.render);
  EXPECT_TRUE(options.preview);
  EXPECT_EQ(options.previewColumns, 32);
  EXPECT_DOUBLE_EQ(options.scale, 0.5);
  EXPECT_TRUE(options.overwrite && options.dryRun && options.verbose);
  EXPECT_TRUE(options.hasHTMLOptions);
  EXPECT_FALSE(options.hasSVGOptions);

  ASSERT_TRUE(ParseImportRequest({"icon.SVG", "--svg-no-expand-use", "--svg-flatten-transforms",
                                  "--svg-preserve-unknown", "--image-base-dir", "images"},
                                 &options, &help, &error));
  EXPECT_FALSE(options.hasHTMLOptions);
  EXPECT_TRUE(options.hasSVGOptions);
  EXPECT_FALSE(options.formatOptions.svgExpandUse);
  EXPECT_TRUE(options.formatOptions.svgFlattenTransforms);
  EXPECT_TRUE(options.formatOptions.svgPreserveUnknown);
  EXPECT_EQ(options.imageBaseDir, "images");
  EXPECT_TRUE(options.cookies.empty());
  EXPECT_FALSE(options.render);
}

CLI_TEST(PAGXImportValidationTest, RejectsMalformedArgumentsWithSpecificDiagnostics) {
  const std::vector<std::pair<std::vector<std::string>, std::string>> cases = {
      {{}, "specify one input"},
      {{""}, "input must not be empty"},
      {{"a.html", "b.html"}, "only one input"},
      {{"a.html", "--output"}, "requires a value"},
      {{"a.html", "--output", ""}, "requires a value"},
      {{"a.html", "--output", "-file"}, "requires a value"},
      {{"a.html", "-v", "--verbose"}, "duplicate option"},
      {{"a.html", "--images", "auto"}, "external or embed"},
      {{"a.html", "--format", "ppt"}, "html or svg"},
      {{"a.txt"}, "unsupported input format"},
      {{"a.html", "--html-source", "auto"}, "browser or snapshot"},
      {{"a.html", "--html-browser-engine", "firefox"}, "unsupported --html-browser-engine"},
      {{"a.svg", "--svg-unknown"}, "unknown option"},
      {{"a.html", "--html-unknown"}, "unknown option"},
      {{"a.svg", "--html-keep-snapshot"}, "HTML options require HTML input"},
      {{"a.svg", "--html-scenario", "s.json", "--output-dir", "out"},
       "HTML options require HTML input"},
      {{"--batch", "input"}, "--batch requires --output-dir"},
      {{"--batch", "input", "--output-dir", "out", "--format", "svg"}, "batch infers"},
      {{"a.html", "--output-name", "a"}, "--output-name requires"},
      {{"a.html", "--html-scenario", "s.json", "--output-dir", "out", "--html-source", "snapshot"},
       "--html-scenario requires --html-source browser"},
      {{"https://example.test", "-o", "a.pagx", "--format", "svg"}, "URL input requires HTML"},
      {{"https://example.test", "-o", "a.pagx", "--html-source", "snapshot"}, "local file"},
      {{"https://example.test", "-o", "a.pagx", "--image-base-dir", "images"}, "local input"},
      {{"a.html", "--render", "--scale", "0"}, "must be positive"},
  };
  for (const auto& test : cases) {
    SCOPED_TRACE(test.second);
    ImportRequest options;
    bool help = false;
    std::string error;
    EXPECT_FALSE(ParseImportRequest(test.first, &options, &help, &error));
    EXPECT_NE(error.find(test.second), std::string::npos) << error;
  }
  for (const auto& flag : {"--html-viewport-width", "--html-viewport-height", "--html-wait-ms",
                           "--preview-columns", "--scale"}) {
    for (const auto& value : {"NaN", "inf", "abc", "1e999", "1x", "1000001"}) {
      SCOPED_TRACE(std::string(flag) + "=" + value);
      ImportRequest options;
      bool help = false;
      std::string error;
      EXPECT_FALSE(
          ParseImportRequest({"a.html", "--preview", flag, value}, &options, &help, &error));
      EXPECT_NE(error.find("invalid number"), std::string::npos) << error;
    }
  }
}

CLI_TEST(PAGXImportValidationTest, LegacyHTMLFlagsReportTheirReplacement) {
  const auto usage = ImportUsage();
  for (const std::string flag :
       {"--scenario", "--checkpoint", "--browser-engine", "--viewport-width", "--viewport-height",
        "--wait-ms", "--selector", "--cookie", "--header", "--download-fonts",
        "--capture-animations", "--scroll-reveal", "--no-inline-icon-fonts", "--keep-snapshot"}) {
    SCOPED_TRACE(flag);
    ImportRequest options;
    bool help = false;
    std::string error;
    EXPECT_FALSE(ParseImportRequest({"a.html", flag}, &options, &help, &error));
    const auto replacement = "--html-" + flag.substr(2);
    EXPECT_EQ(error, flag + " has been renamed to " + replacement);
    EXPECT_NE(usage.find(replacement), std::string::npos);
    EXPECT_EQ(usage.find(flag), std::string::npos);
  }
}

CLI_TEST(PAGXImportValidationTest, HTMLSingletonOptionsRejectDuplicates) {
  for (const auto& extra :
       std::vector<std::vector<std::string>>{{"--html-source", "browser"},
                                             {"--html-scenario", "s.json"},
                                             {"--html-checkpoint", "one"},
                                             {"--html-browser-engine", "puppeteer"},
                                             {"--html-viewport-width", "1400"},
                                             {"--html-viewport-height", "900"},
                                             {"--html-wait-ms", "0"},
                                             {"--html-selector", "#ready"},
                                             {"--html-capture-animations"},
                                             {"--html-scroll-reveal"},
                                             {"--html-no-inline-icon-fonts"},
                                             {"--html-download-fonts"},
                                             {"--html-keep-snapshot"}}) {
    SCOPED_TRACE(extra.front());
    std::vector<std::string> args = {"a.html"};
    args.insert(args.end(), extra.begin(), extra.end());
    args.insert(args.end(), extra.begin(), extra.end());
    ImportRequest options;
    bool help = false;
    std::string error;
    EXPECT_FALSE(ParseImportRequest(args, &options, &help, &error));
    EXPECT_EQ(error, "duplicate option: " + extra.front());
  }
}

CLI_TEST(PAGXImportValidationTest, HTMLNumericBoundsAndScenarioConflictsArePreserved) {
  ImportRequest options;
  bool help = false;
  std::string error;
  ASSERT_TRUE(ParseImportRequest({"a.html", "--html-viewport-width", "1", "--html-viewport-height",
                                  "16384", "--html-wait-ms", "300000"},
                                 &options, &help, &error))
      << error;
  EXPECT_EQ(options.browserOptions.at("viewport-width"), "1");
  EXPECT_EQ(options.browserOptions.at("viewport-height"), "16384");
  EXPECT_EQ(options.browserOptions.at("wait-ms"), "300000");
  for (const auto& extra :
       std::vector<std::vector<std::string>>{{"--html-viewport-width", "0"},
                                             {"--html-viewport-height", "0"},
                                             {"--html-viewport-width", "16385"},
                                             {"--html-viewport-height", "16385"},
                                             {"--html-wait-ms", "300001"},
                                             {"--html-wait-ms", "0.5"}}) {
    std::vector<std::string> args = {"a.html"};
    args.insert(args.end(), extra.begin(), extra.end());
    EXPECT_FALSE(ParseImportRequest(args, &options, &help, &error));
    EXPECT_NE(error.find("invalid number for " + extra.front()), std::string::npos) << error;
  }
  for (const auto& extra : std::vector<std::vector<std::string>>{{"--html-viewport-width", "1400"},
                                                                 {"--html-viewport-height", "900"},
                                                                 {"--html-wait-ms", "0"},
                                                                 {"--html-selector", "#ready"},
                                                                 {"--html-capture-animations"},
                                                                 {"--html-scroll-reveal"}}) {
    std::vector<std::string> args = {"a.html", "--html-scenario", "s.json", "--output-dir", "out"};
    args.insert(args.end(), extra.begin(), extra.end());
    EXPECT_FALSE(ParseImportRequest(args, &options, &help, &error));
    EXPECT_NE(error.find("--html-scenario owns"), std::string::npos) << error;
    EXPECT_NE(error.find(extra.front()), std::string::npos) << error;
  }
}

CLI_TEST(PAGXImportValidationTest, SnapshotRejectsEveryBrowserOnlyOption) {
  for (const auto& extra :
       std::vector<std::vector<std::string>>{{"--html-capture-animations"},
                                             {"--html-scroll-reveal"},
                                             {"--html-download-fonts"},
                                             {"--html-no-inline-icon-fonts"},
                                             {"--html-cookie", "a=b"},
                                             {"--html-header", "A: b"},
                                             {"--html-selector", "#a"},
                                             {"--html-viewport-width", "1"},
                                             {"--html-viewport-height", "1"},
                                             {"--html-wait-ms", "0"},
                                             {"--html-browser-engine", "puppeteer"}}) {
    SCOPED_TRACE(extra.front());
    std::vector<std::string> args = {"a.html", "--html-source", "snapshot"};
    args.insert(args.end(), extra.begin(), extra.end());
    ImportRequest options;
    bool help;
    std::string error;
    EXPECT_FALSE(ParseImportRequest(args, &options, &help, &error));
    EXPECT_NE(error.find("browser options cannot"), std::string::npos) << error;
  }
}

CLI_TEST(PAGXImportValidationTest, PositionalDelimiterAndForcedFormat) {
  ImportRequest options;
  bool help;
  std::string error;
  ASSERT_TRUE(ParseImportRequest({"--format", "html", "--", "--help"}, &options, &help, &error));
  EXPECT_FALSE(help);
  EXPECT_EQ(options.input, "--help");
  ASSERT_TRUE(ParseImportRequest({"-h"}, &options, &help, &error));
  EXPECT_TRUE(help);
  EXPECT_TRUE(options.input.empty());
  EXPECT_TRUE(IsImportURL("http://example.test"));
  EXPECT_TRUE(IsImportURL("https://example.test"));
  EXPECT_FALSE(IsImportURL("file:///page.html"));
}

CLI_TEST(PAGXImportValidationTest, PlansEveryArtifactWithoutCreatingIt) {
  ImportTempDirectory temp;
  auto input = temp.path / "Page.HTM";
  std::ofstream(input) << "<html/>";
  ImportRequest options;
  options.input = input.u8string();
  options.outputDir = (temp.path / "results").u8string();
  options.outputName = "custom";
  options.scenario = (temp.path / "states.json").u8string();
  options.render = options.preview = options.downloadFonts = options.keepSnapshot = true;
  ImportPlan plan;
  std::string error;
  ASSERT_TRUE(BuildImportPlan(options, {"second", "first"}, &plan, &error)) << error;
  ASSERT_EQ(plan.items.size(), 2u);
  auto stem = fs::weakly_canonical(fs::u8path(options.outputDir)) / "custom.second";
  EXPECT_EQ(plan.items[0].format, "html");
  EXPECT_EQ(plan.items[0].output, stem.u8string() + ".pagx");
  EXPECT_EQ(plan.items[0].png, stem.u8string() + ".png");
  EXPECT_EQ(plan.items[0].assets, stem.u8string() + ".assets");
  EXPECT_EQ(plan.items[0].fonts, stem.u8string() + ".fonts");
  EXPECT_EQ(plan.items[0].snapshot, stem.u8string() + ".subset.html");
  EXPECT_EQ(plan.previewDir,
            (fs::weakly_canonical(fs::u8path(options.outputDir)) / "preview").u8string());
  EXPECT_FALSE(fs::exists(fs::u8path(options.outputDir)));

  options.checkpoint = "first";
  options.outputDir.clear();
  options.output = (temp.path / "single.pagx").u8string();
  options.images = ImageStorageMode::Embed;
  ASSERT_TRUE(BuildImportPlan(options, {"second", "first"}, &plan, &error)) << error;
  ASSERT_EQ(plan.items.size(), 1u);
  EXPECT_EQ(plan.items[0].checkpoint, "first");
  EXPECT_EQ(plan.items[0].output, fs::weakly_canonical(fs::u8path(options.output)).u8string());
  EXPECT_TRUE(plan.items[0].assets.empty());
  EXPECT_TRUE(plan.manifest.empty());
  EXPECT_EQ(plan.previewDir, (fs::canonical(temp.path) / "single.preview").u8string());
}

CLI_TEST(PAGXImportValidationTest, RejectsMissingInputsAndInvalidCheckpointSets) {
  ImportTempDirectory temp;
  ImportRequest options;
  options.input = (temp.path / "absent.html").u8string();
  ImportPlan plan;
  std::string error;
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("input not found"), std::string::npos);
  std::ofstream(options.input) << "<html/>";
  options.scenario = (temp.path / "states.json").u8string();
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("no capture checkpoints"), std::string::npos);
  for (const auto& names : std::vector<std::vector<std::string>>{{"../escape"},
                                                                 {"aux"},
                                                                 {"first", "first"},
                                                                 {"first", "FIRST"}}) {
    EXPECT_FALSE(BuildImportPlan(options, names, &plan, &error));
    EXPECT_FALSE(error.empty());
  }
  options.output = options.scenario;
  options.overwrite = true;
  EXPECT_FALSE(BuildImportPlan(options, {"first"}, &plan, &error));
  EXPECT_NE(error.find("replace an input"), std::string::npos);
}

CLI_TEST(PAGXImportValidationTest, BatchSkipsItsOutputsAndPreparedSnapshots) {
  ImportTempDirectory temp;
  auto input = temp.path / "input", output = input / "results";
  fs::create_directories(output);
  fs::create_directory(input / "nested");
  std::ofstream(input / "b.SVG") << "<svg/>";
  std::ofstream(input / "nested/a.HTM") << "<html/>";
  std::ofstream(input / "old.SUBSET.HTML") << "<html/>";
  std::ofstream(input / "ignored.txt") << "text";
  std::ofstream(output / "generated.html") << "<html/>";
  ImportRequest options;
  options.batch = input.u8string();
  options.outputDir = output.u8string();
  ImportPlan plan;
  std::string error;
  ASSERT_TRUE(BuildImportPlan(options, {}, &plan, &error)) << error;
  ASSERT_EQ(plan.items.size(), 2u);
  EXPECT_EQ(plan.items[0].input, (fs::canonical(input) / "b.SVG").u8string());
  EXPECT_EQ(plan.items[1].output, (fs::canonical(output) / "nested/a.pagx").u8string());
  options.hasHTMLOptions = true;
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("batch SVG"), std::string::npos);
  options.hasHTMLOptions = false;
  options.hasSVGOptions = true;
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("batch HTML"), std::string::npos);
  options.hasSVGOptions = false;
  options.outputDir = options.batch;
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("must differ"), std::string::npos);
  options.outputDir = (temp.path / "out").u8string();
  options.batch = (temp.path / "missing").u8string();
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("directory not found"), std::string::npos);
  fs::create_directory(fs::u8path(options.batch));
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("no supported inputs"), std::string::npos);
}

CLI_TEST(PAGXImportValidationTest, RejectsWrongOutputTypesAndNonDirectoryParents) {
  ImportTempDirectory temp;
  auto input = temp.path / "page.svg";
  std::ofstream(input) << "<svg/>";
  ImportRequest options;
  options.input = input.u8string();
  options.overwrite = true;
  ImportPlan plan;
  std::string error;
  fs::create_directory(temp.path / "page.pagx");
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("invalid output type"), std::string::npos);
  fs::remove(temp.path / "page.pagx");
  std::ofstream(temp.path / "page.assets") << "not a directory";
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("invalid output type"), std::string::npos);
  options.output = (temp.path / "page.assets/child.pagx").u8string();
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("parent is not a directory"), std::string::npos);
  options.output.clear();
  fs::remove(temp.path / "page.assets");
  fs::create_directory(temp.path / "page.assets");
  EXPECT_TRUE(BuildImportPlan(options, {}, &plan, &error)) << error;
  options.overwrite = false;
  EXPECT_FALSE(BuildImportPlan(options, {}, &plan, &error));
  EXPECT_NE(error.find("--overwrite"), std::string::npos);
}
}  // namespace pag
