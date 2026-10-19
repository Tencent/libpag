// Copyright (C) 2026 Tencent. Licensed under the Apache License, Version 2.0.
#include <filesystem>
#include <fstream>
#include "base/PAGTest.h"
#include "cli/ImportPlan.h"
#include "utils/PAGXTestUtils.h"
#include "utils/TestDir.h"

namespace pag {
namespace fs = std::filesystem;
using namespace pagx::cli;

static bool Parse(const std::vector<std::string>& args, ImportOptions* options,
                  std::string* error = nullptr) {
  std::string diagnostic;
  bool help = false;
  return ParseImportOptions(args, options, &help, error ? error : &diagnostic) && !help;
}

CLI_TEST(PAGXImportPlanTest, ExplicitOutputAndDefaults) {
  ImportOptions o;
  ASSERT_TRUE(Parse({"page.html", "-o", "out.pagx"}, &o));
  EXPECT_EQ(o.output, "out.pagx");
  EXPECT_TRUE(o.outputDir.empty());
  EXPECT_FALSE(o.render);
  EXPECT_FALSE(o.keepSnapshot);
  EXPECT_TRUE(o.resolve);
  EXPECT_FALSE(Parse({"page.html", "-o", "out.pagx", "--output-dir", "out"}, &o));
  EXPECT_FALSE(Parse({"page.html", "--input", "other.html"}, &o));
  EXPECT_FALSE(Parse({"page.html", "--pagx-images", "embed"}, &o));
  EXPECT_FALSE(Parse({"page.html", "--no-render"}, &o));
  EXPECT_FALSE(Parse({"page.html", "-o", "one", "--output", "two"}, &o));
}

CLI_TEST(PAGXImportPlanTest, CardinalityDoesNotDependOnScenarioContents) {
  ImportOptions o;
  EXPECT_FALSE(Parse({"page.html", "--scenario", "states.json", "-o", "one.pagx"}, &o));
  EXPECT_TRUE(Parse({"page.html", "--scenario", "states.json", "--output-dir", "out"}, &o));
  EXPECT_TRUE(Parse(
      {"page.html", "--scenario", "states.json", "--checkpoint", "open", "-o", "one.pagx"}, &o));
  EXPECT_FALSE(Parse({"page.html", "--checkpoint", "open"}, &o));
  EXPECT_FALSE(Parse({"--batch", "pages", "--output-dir", "out", "--scenario", "s.json"}, &o));
}

CLI_TEST(PAGXImportPlanTest, DependenciesAndFormatsAreValidated) {
  ImportOptions o;
  ASSERT_TRUE(Parse({"page.html", "--preview"}, &o));
  EXPECT_TRUE(o.render);
  for (const auto& args : std::vector<std::vector<std::string>>{
           {"page.html", "--no-resolve", "--preview"},
           {"page.html", "--render", "--no-resolve"},
           {"page.html", "--embed-fonts", "--no-resolve"},
           {"page.html", "--scale", "2"},
           {"page.html", "--render", "--scale", "nan"},
           {"page.html", "--render", "--scale", "2x"},
           {"page.html", "--preview-columns", "2"},
           {"page.html", "--preview", "--preview-columns", "1.5"},
           {"icon.svg", "--browser-engine", "puppeteer"},
           {"page.html", "--svg-no-expand-use"},
           {"page.html", "--cookie", "a=b"},
           {"page.html", "--html-source", "snapshot", "--scroll-reveal"},
           {"page.html", "--scenario", "s.json", "--output-dir", "out", "--wait-ms", "0"}}) {
    std::string error;
    EXPECT_FALSE(Parse(args, &o, &error)) << args.back();
    EXPECT_FALSE(error.empty());
  }
}

CLI_TEST(PAGXImportPlanTest, URLNamingAndPortableBasenames) {
  ImportOptions o;
  EXPECT_FALSE(Parse({"https://example.com"}, &o));
  EXPECT_FALSE(Parse({"https://example.com", "--output-dir", "out"}, &o));
  EXPECT_TRUE(Parse({"https://example.com", "-o", "web.pagx"}, &o));
  EXPECT_TRUE(Parse({"https://example.com", "--output-dir", "out", "--output-name", "web"}, &o));
  for (const auto& name : {"..", "a/b", "a\\b", "a:", "a."})
    EXPECT_FALSE(Parse({"page.html", "--output-dir", "out", "--output-name", name}, &o));
}

CLI_TEST(PAGXImportPlanTest, HelpNeedsNoInputOrRuntime) {
  ImportOptions o;
  bool help = false;
  std::string error;
  EXPECT_TRUE(ParseImportOptions({"--help"}, &o, &help, &error));
  EXPECT_TRUE(help);
  EXPECT_NE(ImportUsage().find("--output-dir"), std::string::npos);
}

CLI_TEST(PAGXImportPlanTest, BatchCollisionsAndDryPlanning) {
  auto root = fs::path(TestDir::GetRoot()) / "out/PAGXImportPlanTest";
  fs::remove_all(root);
  fs::create_directories(root / "input/sub");
  std::ofstream(root / "input/a.html") << "<html/>";
  std::ofstream(root / "input/a.svg") << "<svg/>";
  std::ofstream(root / "input/sub/b.svg") << "<svg/>";
  auto output = (root / "results").u8string();
  ImportOptions o;
  ASSERT_TRUE(
      Parse({"--batch", (root / "input").u8string(), "--output-dir", output, "--dry-run"}, &o));
  ImportPlan plan;
  std::string error;
  EXPECT_FALSE(BuildImportPlan(o, {}, &plan, &error));
  EXPECT_NE(error.find("collision"), std::string::npos);
  EXPECT_FALSE(fs::exists(root / "results"));
  fs::remove(root / "input/a.svg");
  ASSERT_TRUE(BuildImportPlan(o, {}, &plan, &error)) << error;
  ASSERT_EQ(plan.items.size(), 2u);
  EXPECT_EQ(fs::u8path(plan.items[1].output).filename(), "b.pagx");
  EXPECT_EQ(fs::u8path(plan.items[1].output).parent_path().filename(), "sub");
  EXPECT_FALSE(fs::exists(root / "results"));
  fs::create_directories(root / "results");
  std::ofstream(root / "results/a.pagx") << "old";
  EXPECT_FALSE(BuildImportPlan(o, {}, &plan, &error));
  EXPECT_NE(error.find("--overwrite"), std::string::npos);
  o.overwrite = true;
  EXPECT_TRUE(BuildImportPlan(o, {}, &plan, &error));
  fs::remove_all(root);
}

CLI_TEST(PAGXImportPlanTest, PlannedCheckpointOrderAndSourceProtection) {
  auto root = fs::path(TestDir::GetRoot()) / "out/PAGXImportPlanStates";
  fs::remove_all(root);
  fs::create_directories(root);
  auto input = (root / "page.html").u8string();
  std::ofstream(input) << "<html/>";
  ImportOptions o;
  ASSERT_TRUE(
      Parse({input, "--scenario", "s.json", "--output-dir", (root / "out").u8string()}, &o));
  ImportPlan plan;
  std::string error;
  ASSERT_TRUE(BuildImportPlan(o, {"z", "a"}, &plan, &error)) << error;
  ASSERT_EQ(plan.items.size(), 2u);
  EXPECT_EQ(plan.items[0].checkpoint, "z");
  EXPECT_EQ(plan.items[1].checkpoint, "a");
  o.checkpoint = "absent";
  EXPECT_FALSE(BuildImportPlan(o, {"z", "a"}, &plan, &error));
  ASSERT_TRUE(Parse({input, "-o", input, "--overwrite"}, &o));
  EXPECT_FALSE(BuildImportPlan(o, {}, &plan, &error));
  EXPECT_NE(error.find("replace an input"), std::string::npos);
  fs::remove_all(root);
}
}  // namespace pag
