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

#include <algorithm>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/HTMLExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/ColorStop.h"
#include "pagx/nodes/DiamondGradient.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/types/Data.h"
#include "utils/PAGXImageTestUtils.h"
#include "utils/ProjectPath.h"
#include "utils/ZipTestUtils.h"

namespace pag {

namespace {

static std::string FindEntryWithPrefix(const std::unordered_map<std::string, std::string>& entries,
                                       const std::string& prefix) {
  for (const auto& entry : entries) {
    if (entry.first.rfind(prefix, 0) == 0) {
      return entry.first;
    }
  }
  return {};
}

static size_t CountEntriesWithPrefix(const std::unordered_map<std::string, std::string>& entries,
                                     const std::string& prefix) {
  return static_cast<size_t>(std::count_if(entries.begin(), entries.end(), [&](const auto& entry) {
    return entry.first.rfind(prefix, 0) == 0;
  }));
}

// A document with a single centered rectangle and no fill.
static std::shared_ptr<pagx::PAGXDocument> MakeSimpleDoc() {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 100};
  layer->contents.push_back(rect);
  doc->layers.push_back(layer);
  return doc;
}

// Appends an ImagePattern fill to `layer` so the rectangle is painted with the given image.
static void AppendImagePatternFill(pagx::Layer* layer, pagx::PAGXDocument* doc,
                                   pagx::Image* image) {
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  pattern->matrix = {1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f};
  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = pattern;
  layer->contents.push_back(fill);
}

// Sorted names of every entry (file or directory) directly inside the working directory.
static std::vector<std::string> ListWorkingDirectory() {
  std::vector<std::string> names;
  for (const auto& entry : std::filesystem::directory_iterator(std::filesystem::current_path())) {
    names.push_back(entry.path().filename().string());
  }
  std::sort(names.begin(), names.end());
  return names;
}

}  // namespace

// ToData returns a complete archive whose central directory, entry payloads, and
// CRCs can all be read successfully.
PAGX_TEST(PAGXHTMLDataTest, ToData_BasicArchive) {
  auto doc = MakeSimpleDoc();
  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ExtractZipEntries(data.get(), &entries, &error)) << error;
  ASSERT_EQ(entries.count("index.html"), 1u);
  EXPECT_NE(entries.at("index.html").find("<!DOCTYPE html>"), std::string::npos);
}

// An Image node whose bytes are provided via image->data is embedded as an assets/img*.png
// entry; the HTML references it under the "assets/" URL prefix.
PAGX_TEST(PAGXHTMLDataTest, ToData_WithEmbeddedImage) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};
  layer->contents.push_back(rect);
  auto* image = MakeTestPNGImage(doc.get());
  AppendImagePatternFill(layer, doc.get(), image);
  doc->layers.push_back(layer);

  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ExtractZipEntries(data.get(), &entries, &error)) << error;
  auto imageEntry = FindEntryWithPrefix(entries, "assets/img");
  ASSERT_FALSE(imageEntry.empty());
  EXPECT_FALSE(entries.at(imageEntry).empty());
  EXPECT_NE(entries.at("index.html").find(imageEntry), std::string::npos);
}

// An Image referenced only by a "hash:" filePath has no readable bytes, so the image degrades
// to an empty src: the archive stays valid and contains no img entry.
PAGX_TEST(PAGXHTMLDataTest, ToData_MissingHashImage) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};
  layer->contents.push_back(rect);
  auto* image = doc->makeNode<pagx::Image>();
  image->filePath = "hash:abc";
  AppendImagePatternFill(layer, doc.get(), image);
  doc->layers.push_back(layer);

  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ExtractZipEntries(data.get(), &entries, &error)) << error;
  EXPECT_TRUE(FindEntryWithPrefix(entries, "assets/img").empty());
}

// ToData is an in-memory export: nothing is written to the working directory, so the directory
// listing is unchanged before and after the call.
PAGX_TEST(PAGXHTMLDataTest, ToData_NoFilesWritten) {
  auto doc = MakeSimpleDoc();
  auto before = ListWorkingDirectory();

  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ExtractZipEntries(data.get(), &entries, &error)) << error;

  EXPECT_EQ(ListWorkingDirectory(), before);
}

// A DiamondGradient fill has no CSS equivalent and must be rasterized into an assets/dgc*.png
// entry, proving the writeResource path feeds the in-memory archive.
PAGX_TEST(PAGXHTMLDataTest, ToData_DiamondRasterized) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 200};
  layer->contents.push_back(rect);

  auto* grad = doc->makeNode<pagx::DiamondGradient>();
  grad->center = {200, 150};
  grad->radius = 100;
  auto* s0 = doc->makeNode<pagx::ColorStop>();
  s0->offset = 0;
  s0->color = {1.0f, 0.8f, 0.0f, 1.0f};
  auto* s1 = doc->makeNode<pagx::ColorStop>();
  s1->offset = 1;
  s1->color = {0.0f, 0.2f, 0.6f, 1.0f};
  grad->colorStops.push_back(s0);
  grad->colorStops.push_back(s1);

  auto* fill = doc->makeNode<pagx::Fill>();
  fill->color = grad;
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);

  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ExtractZipEntries(data.get(), &entries, &error)) << error;
  auto imageEntry = FindEntryWithPrefix(entries, "assets/dgc");
  ASSERT_FALSE(imageEntry.empty());
  EXPECT_NE(entries.at("index.html").find(imageEntry), std::string::npos);
}

PAGX_TEST(PAGXHTMLDataTest, ToData_ReadsLocalImageFile) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* layer = doc->makeNode<pagx::Layer>();
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->position = {200, 150};
  rect->size = {200, 150};
  layer->contents.push_back(rect);
  auto* image = doc->makeNode<pagx::Image>();
  image->filePath = ProjectPath::Absolute("resources/apitest/imageReplacement.png");
  AppendImagePatternFill(layer, doc.get(), image);
  doc->layers.push_back(layer);

  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr) << error;
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ExtractZipEntries(data.get(), &entries, &error)) << error;
  auto imageEntry = FindEntryWithPrefix(entries, "assets/img");
  ASSERT_FALSE(imageEntry.empty());
  EXPECT_NE(entries.at("index.html").find(imageEntry), std::string::npos);
}

PAGX_TEST(PAGXHTMLDataTest, ToData_DeduplicatesSharedImage) {
  auto doc = pagx::PAGXDocument::Make(400, 300);
  auto* image = MakeTestPNGImage(doc.get());
  for (float x : {100.0f, 300.0f}) {
    auto* layer = doc->makeNode<pagx::Layer>();
    auto* rect = doc->makeNode<pagx::Rectangle>();
    rect->position = {x, 150};
    rect->size = {100, 100};
    layer->contents.push_back(rect);
    AppendImagePatternFill(layer, doc.get(), image);
    doc->layers.push_back(layer);
  }

  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr) << error;
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ExtractZipEntries(data.get(), &entries, &error)) << error;
  EXPECT_EQ(CountEntriesWithPrefix(entries, "assets/img"), 1u);
}

PAGX_TEST(PAGXHTMLDataTest, ToData_EmbedsGeneratedFont) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/unit/glyph_run_embedded_font.pagx"));
  ASSERT_NE(doc, nullptr);

  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr) << error;
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ExtractZipEntries(data.get(), &entries, &error)) << error;
  auto fontEntry = FindEntryWithPrefix(entries, "assets/fonts/font_");
  ASSERT_FALSE(fontEntry.empty());
  EXPECT_FALSE(entries.at(fontEntry).empty());
  EXPECT_NE(entries.at("index.html").find(fontEntry), std::string::npos);
}

PAGX_TEST(PAGXHTMLDataTest, ToData_InlinesPlusDarkerBackdrop) {
  auto doc = pagx::PAGXImporter::FromFile(
      ProjectPath::Absolute("resources/pagx_to_html/layer_blend_modes.pagx"));
  ASSERT_NE(doc, nullptr);

  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr) << error;
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ExtractZipEntries(data.get(), &entries, &error)) << error;
  EXPECT_TRUE(FindEntryWithPrefix(entries, "assets/pd_").empty());
  const auto& html = entries.at("index.html");
  EXPECT_NE(html.find("pagx_pd_"), std::string::npos);
  EXPECT_NE(html.find("data:image/png;base64,"), std::string::npos);
}

}  // namespace pag
