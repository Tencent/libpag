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

namespace pag {

namespace {

static bool HasZipMagic(const pagx::Data* data) {
  if (data == nullptr || data->size() < 4) {
    return false;
  }
  const auto* bytes = data->bytes();
  return bytes[0] == 0x50 && bytes[1] == 0x4B && bytes[2] == 0x03 && bytes[3] == 0x04;
}

// Minimal valid 2x2 RGBA PNG (8-bit, non-interlaced).
static pagx::Image* MakeTestPNGImage(pagx::PAGXDocument* doc) {
  static const uint8_t MINIMAL_PNG[] = {
      0x89,
      0x50,
      0x4E,
      0x47,
      0x0D,
      0x0A,
      0x1A,
      0x0A,  // PNG signature
      // IHDR
      0x00,
      0x00,
      0x00,
      0x0D,
      0x49,
      0x48,
      0x44,
      0x52,
      0x00,
      0x00,
      0x00,
      0x02,
      0x00,
      0x00,
      0x00,
      0x02,
      0x08,
      0x02,
      0x00,
      0x00,
      0x00,
      0xFD,
      0xD4,
      0x9A,
      0x73,
      // IDAT (compressed pixel data)
      0x00,
      0x00,
      0x00,
      0x14,
      0x49,
      0x44,
      0x41,
      0x54,
      0x78,
      0x9C,
      0x62,
      0xF8,
      0xCF,
      0xC0,
      0xF0,
      0x1F,
      0x01,
      0x18,
      0x18,
      0x18,
      0x00,
      0x09,
      0x04,
      0x01,
      0x01,
      0xE2,
      0x2D,
      0x42,
      0xA3,
      // IEND
      0x00,
      0x00,
      0x00,
      0x00,
      0x49,
      0x45,
      0x4E,
      0x44,
      0xAE,
      0x42,
      0x60,
      0x82,
  };
  auto* image = doc->makeNode<pagx::Image>();
  image->data = pagx::Data::MakeWithCopy(MINIMAL_PNG, sizeof(MINIMAL_PNG));
  return image;
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

// ToData returns a non-null ZIP buffer whose entry names (index.html plus assets) appear
// verbatim in the raw bytes. Entry content is DEFLATE-compressed, so only entry names can be
// spot-checked as plain text.
PAGX_TEST(PAGXHTMLDataTest, ToData_BasicArchive) {
  auto doc = MakeSimpleDoc();
  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr);
  EXPECT_TRUE(HasZipMagic(data.get()));

  std::string bytes(reinterpret_cast<const char*>(data->bytes()), data->size());
  EXPECT_NE(bytes.find("index.html"), std::string::npos);
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
  EXPECT_TRUE(HasZipMagic(data.get()));

  std::string bytes(reinterpret_cast<const char*>(data->bytes()), data->size());
  EXPECT_NE(bytes.find("assets/img"), std::string::npos);
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
  EXPECT_TRUE(HasZipMagic(data.get()));

  std::string bytes(reinterpret_cast<const char*>(data->bytes()), data->size());
  EXPECT_EQ(bytes.find("assets/img"), std::string::npos);
}

// ToData is an in-memory export: nothing is written to the working directory, so the directory
// listing is unchanged before and after the call.
PAGX_TEST(PAGXHTMLDataTest, ToData_NoFilesWritten) {
  auto doc = MakeSimpleDoc();
  auto before = ListWorkingDirectory();

  std::string error;
  auto data = pagx::HTMLExporter::ToData(*doc, {}, &error);
  ASSERT_NE(data, nullptr);

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
  EXPECT_TRUE(HasZipMagic(data.get()));

  std::string bytes(reinterpret_cast<const char*>(data->bytes()), data->size());
  EXPECT_NE(bytes.find("assets/dgc"), std::string::npos);
}

}  // namespace pag
