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
//  license is distributed on an "AS IS" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>
#include "base/PAGTest.h"
#include "pagx/PAGScene.h"
#include "pagx/PAGSurface.h"
#include "pagx/PAGXDocument.h"
#include "pagx/PAGXExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Font.h"
#include "pagx/nodes/Image.h"
#include "pagx/nodes/ImagePattern.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Rectangle.h"
#include "pagx/types/Data.h"
#include "pagx/utils/ImageMime.h"
#include "tgfx/core/Bitmap.h"
#include "tgfx/core/ImageCodec.h"
#include "tgfx/core/Pixmap.h"
#include "utils/ZipTestUtils.h"

namespace pag {
namespace {

// A decodable 8x8 opaque red PNG. Export tests need real bytes whose format can be sniffed and
// decoded, without depending on runtime encoders or files outside the repository.
const uint8_t kRedPng[] = {
    0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44,
    0x52, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x08, 0x08, 0x06, 0x00, 0x00, 0x00, 0xC4,
    0x0F, 0xBE, 0x8B, 0x00, 0x00, 0x00, 0x12, 0x49, 0x44, 0x41, 0x54, 0x78, 0xDA, 0x63, 0xF8,
    0xCF, 0xC0, 0xF0, 0x1F, 0x1F, 0x66, 0x18, 0x19, 0x0A, 0x00, 0xC2, 0xD7, 0x7F, 0x81, 0xD5,
    0x03, 0x32, 0xFD, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82};

const uint8_t kJpegBytes[] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10, 0x4A, 0x46, 0x49, 0x46};
// Smallest ISO-BMFF header that names the `avif` brand, which is what DetectImageMime reads.
const uint8_t kAvifBytes[] = {0x00, 0x00, 0x00, 0x14, 'f',  't',  'y',  'p',
                              'a',  'v',  'i',  'f',  0x00, 0x00, 0x00, 0x00};
const uint8_t kSvgBytes[] = {'<', 's', 'v', 'g', ' ', 'x', 'm', 'l', 'n', 's', '=', '"', '"', '>'};
const uint8_t kWebpBytes[] = {'R', 'I', 'F', 'F', 0x00, 0x00, 0x00, 0x00,
                              'W', 'E', 'B', 'P', 'V',  'P',  '8',  ' '};
const uint8_t kGifBytes[] = {'G', 'I', 'F', '8', '9', 'a', 0x01, 0x00, 0x01, 0x00};

// Returns the red PNG payload, optionally followed by a marker byte so two images can be told
// apart after extraction without changing the sniffed format.
std::shared_ptr<pagx::Data> MakeImageData(uint8_t marker = 0) {
  std::vector<uint8_t> bytes(kRedPng, kRedPng + sizeof(kRedPng));
  if (marker != 0) {
    bytes.push_back(marker);
  }
  return pagx::Data::MakeWithCopy(bytes.data(), bytes.size());
}

std::shared_ptr<pagx::Data> MakeBytesData(const uint8_t* bytes, size_t size) {
  return pagx::Data::MakeWithCopy(bytes, size);
}

pagx::Image* AddImage(pagx::PAGXDocument* doc, const std::string& id,
                      const std::string& filePath = {},
                      std::shared_ptr<pagx::Data> data = nullptr) {
  auto* image = doc->makeNode<pagx::Image>(id);
  image->filePath = filePath;
  image->data = std::move(data);
  return image;
}

// Adds a layer whose single 100x100 rectangle is filled with the given image, so the image takes
// part in serialization as an ImagePattern reference.
void AddPatternLayer(pagx::PAGXDocument* doc, pagx::Image* image, float size = 100.0f) {
  auto* layer = doc->makeNode<pagx::Layer>();
  layer->width = size;
  layer->height = size;
  auto* rect = doc->makeNode<pagx::Rectangle>();
  rect->size = {size, size};
  auto* fill = doc->makeNode<pagx::Fill>();
  auto* pattern = doc->makeNode<pagx::ImagePattern>();
  pattern->image = image;
  fill->color = pattern;
  layer->contents.push_back(rect);
  layer->contents.push_back(fill);
  doc->layers.push_back(layer);
}

std::string BytesOf(const std::shared_ptr<pagx::Data>& data) {
  return std::string(reinterpret_cast<const char*>(data->bytes()), data->size());
}

bool ReadZip(const std::shared_ptr<pagx::Data>& data,
             std::unordered_map<std::string, std::string>* entries) {
  std::string error;
  bool ok = ExtractZipEntries(data.get(), entries, &error);
  EXPECT_TRUE(ok) << error;
  return ok;
}

size_t CountOccurrences(const std::string& text, const std::string& needle) {
  size_t count = 0;
  for (size_t pos = text.find(needle); pos != std::string::npos; pos = text.find(needle, pos + 1)) {
    count++;
  }
  return count;
}

std::string ReadFileBytes(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return {};
  }
  return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

bool WriteEntriesToDisk(const std::unordered_map<std::string, std::string>& entries,
                        const std::filesystem::path& directory) {
  std::error_code error;
  for (const auto& [name, content] : entries) {
    auto path = directory / name;
    std::filesystem::create_directories(path.parent_path(), error);
    std::ofstream file(path, std::ios::binary);
    if (!file) {
      return false;
    }
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!file.good()) {
      return false;
    }
  }
  return true;
}

size_t CountOpaquePixels(const std::shared_ptr<pagx::PAGXDocument>& doc, int size) {
  auto scene = pagx::PAGScene::Make(doc);
  if (scene == nullptr) {
    return 0;
  }
  auto surface = pagx::PAGSurface::MakeOffscreen(size, size);
  if (surface == nullptr || !scene->draw(surface)) {
    return 0;
  }
  tgfx::Bitmap bitmap(size, size, false, false);
  if (bitmap.isEmpty()) {
    return 0;
  }
  tgfx::Pixmap pixmap(bitmap);
  if (!surface->readPixels(pixmap.writablePixels(), pixmap.rowBytes())) {
    return 0;
  }
  const auto* pixels = static_cast<const uint8_t*>(pixmap.writablePixels());
  size_t opaque = 0;
  for (size_t i = 3; i < static_cast<size_t>(pixmap.rowBytes()) * size; i += 4) {
    if (pixels[i] != 0) {
      opaque++;
    }
  }
  return opaque;
}

}  // namespace

PAGX_TEST(PAGXExporterTest, ToXMLKeepsImageSourcePriority) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  AddImage(doc.get(), "img_path", "assets/photo.png", MakeImageData());
  AddImage(doc.get(), "img_data", "", MakeImageData());
  AddImage(doc.get(), "img_empty");
  auto* anonymous = AddImage(doc.get(), "", "", MakeImageData());
  AddPatternLayer(doc.get(), anonymous);
  auto* pathOnly = AddImage(doc.get(), "", "images/external.png");
  AddPatternLayer(doc.get(), pathOnly);

  auto xml = pagx::PAGXExporter::ToXML(*doc);

  EXPECT_NE(xml.find("id=\"img_path\" source=\"assets/photo.png\""), std::string::npos);
  EXPECT_NE(xml.find("id=\"img_data\" source=\"data:image/png;base64,"), std::string::npos);
  EXPECT_NE(xml.find("id=\"img_empty\" source=\"\""), std::string::npos);
  EXPECT_NE(xml.find("image=\"data:image/png;base64,"), std::string::npos);
  EXPECT_NE(xml.find("image=\"images/external.png\""), std::string::npos);
}

PAGX_TEST(PAGXExporterTest, ToDataPacksDocumentAndImage) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto imageData = MakeImageData();
  auto* image = AddImage(doc.get(), "img_1", "", imageData);
  AddPatternLayer(doc.get(), image);

  auto zip = pagx::PAGXExporter::ToData(*doc);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_EQ(entries.size(), 2u);
  ASSERT_NE(entries.find("document.pagx"), entries.end());
  ASSERT_NE(entries.find("images/img_1.png"), entries.end());

  const auto& xml = entries["document.pagx"];
  EXPECT_NE(xml.find("source=\"images/img_1.png\""), std::string::npos);
  EXPECT_EQ(xml.find("data:"), std::string::npos);
  EXPECT_EQ(entries["images/img_1.png"], BytesOf(imageData));
}

PAGX_TEST(PAGXExporterTest, ToDataKeepsSharedIdReferenceAndWritesOneEntry) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "img_shared", "", MakeImageData());
  AddPatternLayer(doc.get(), image);
  AddPatternLayer(doc.get(), image);

  auto xml = pagx::PAGXExporter::ToXML(*doc);
  EXPECT_EQ(CountOccurrences(xml, "image=\"@img_shared\""), 2u);
  EXPECT_EQ(CountOccurrences(xml, "source=\"data:"), 1u);

  auto zip = pagx::PAGXExporter::ToData(*doc);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_EQ(entries.size(), 2u);
  ASSERT_NE(entries.find("images/img_shared.png"), entries.end());

  auto reloaded = pagx::PAGXImporter::FromXML(entries["document.pagx"]);
  ASSERT_NE(reloaded, nullptr);
  auto* imageNode = reloaded->findNode<pagx::Image>("img_shared");
  ASSERT_NE(imageNode, nullptr);
  size_t patternCount = 0;
  for (const auto& node : reloaded->nodes) {
    if (node->nodeType() != pagx::NodeType::ImagePattern) {
      continue;
    }
    patternCount++;
    EXPECT_EQ(static_cast<const pagx::ImagePattern*>(node.get())->image, imageNode);
  }
  EXPECT_EQ(patternCount, 2u);
}

PAGX_TEST(PAGXExporterTest, ToDataPacksAnonymousAndEmojiImagesOnce) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* anonymous = AddImage(doc.get(), "", "", MakeImageData());
  AddPatternLayer(doc.get(), anonymous);
  AddPatternLayer(doc.get(), anonymous);

  auto* emoji = doc->makeNode<pagx::Image>();
  emoji->filePath = "emoji:wave";
  emoji->data = MakeImageData();
  auto* font = doc->makeNode<pagx::Font>("font_1");
  auto* glyph = doc->makeNode<pagx::Glyph>();
  glyph->image = emoji;
  glyph->advance = 100;
  font->glyphs.push_back(glyph);

  auto zip = pagx::PAGXExporter::ToData(*doc);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_EQ(entries.size(), 3u);
  ASSERT_NE(entries.find("images/img_1.png"), entries.end());
  ASSERT_NE(entries.find("images/emoji_wave.png"), entries.end());
  EXPECT_EQ(CountOccurrences(entries["document.pagx"], "images/img_1.png"), 2u);
  EXPECT_NE(entries["document.pagx"].find("image=\"images/emoji_wave.png\""), std::string::npos);
}

PAGX_TEST(PAGXExporterTest, ToDataResolvesEntryNameCollisions) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* anonymous = AddImage(doc.get(), "", "", MakeImageData(0xA1));
  AddPatternLayer(doc.get(), anonymous);
  AddImage(doc.get(), "img_1", "", MakeImageData(0xA2));
  AddImage(doc.get(), "a.b", "", MakeImageData(0xA3));
  AddImage(doc.get(), "a_b", "", MakeImageData(0xA4));
  AddImage(doc.get(), "A", "", MakeImageData(0xA5));
  AddImage(doc.get(), "a", "", MakeImageData(0xA6));
  AddImage(doc.get(), "a_b_2", "", MakeImageData(0xA7));
  AddImage(doc.get(), "../evil", "", MakeImageData(0xA8));

  auto zip = pagx::PAGXExporter::ToData(*doc);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_EQ(entries.size(), 9u);

  EXPECT_EQ(entries["images/img_1.png"], BytesOf(MakeImageData(0xA1)));
  EXPECT_EQ(entries["images/img_1_2.png"], BytesOf(MakeImageData(0xA2)));
  EXPECT_EQ(entries["images/a_b.png"], BytesOf(MakeImageData(0xA3)));
  EXPECT_EQ(entries["images/a_b_2.png"], BytesOf(MakeImageData(0xA4)));
  EXPECT_EQ(entries["images/A.png"], BytesOf(MakeImageData(0xA5)));
  EXPECT_EQ(entries["images/a_2.png"], BytesOf(MakeImageData(0xA6)));
  EXPECT_EQ(entries["images/a_b_2_2.png"], BytesOf(MakeImageData(0xA7)));
  // `.` and `/` are replaced, so an id cannot create a directory or escape images/.
  EXPECT_EQ(entries["images/___evil.png"], BytesOf(MakeImageData(0xA8)));
  for (const auto& [name, content] : entries) {
    EXPECT_EQ(name.find(".."), std::string::npos) << name;
  }
}

PAGX_TEST(PAGXExporterTest, ToDataAvoidsPathsKeptByUnpackedImages) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto packedData = MakeImageData(0xB1);
  AddImage(doc.get(), "img_1", "", packedData);
  // No data: this image keeps referring to its external path, which normalizes (and lowercases)
  // to the same archive path the packed image would otherwise take.
  auto* kept = AddImage(doc.get(), "", "./Images/./img_1.PNG");
  AddPatternLayer(doc.get(), kept);

  auto zip = pagx::PAGXExporter::ToData(*doc);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries.find("images/img_1.png"), entries.end());
  ASSERT_NE(entries.find("images/img_1_2.png"), entries.end());
  EXPECT_EQ(entries["images/img_1_2.png"], BytesOf(packedData));

  const auto& xml = entries["document.pagx"];
  EXPECT_NE(xml.find("image=\"./Images/./img_1.PNG\""), std::string::npos);
  EXPECT_NE(xml.find("source=\"images/img_1_2.png\""), std::string::npos);
}

PAGX_TEST(PAGXExporterTest, ToDataAvoidsDirectoriesRequiredByKeptImages) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto packedData = MakeImageData(0xD1);
  AddImage(doc.get(), "foo", "", packedData);
  // This kept reference needs images/foo.png to stay a directory after extraction, so the packed
  // image with id "foo" must not claim that name as a file. The kept path is also upper-cased to
  // cover the case-insensitive comparison.
  auto* kept = AddImage(doc.get(), "", "images/Foo.png/photo.png");
  AddPatternLayer(doc.get(), kept);

  auto zip = pagx::PAGXExporter::ToData(*doc);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_EQ(entries.size(), 2u);
  EXPECT_EQ(entries.find("images/foo.png"), entries.end());
  ASSERT_NE(entries.find("images/foo_2.png"), entries.end());
  EXPECT_EQ(entries["images/foo_2.png"], BytesOf(packedData));

  const auto& xml = entries["document.pagx"];
  EXPECT_NE(xml.find("image=\"images/Foo.png/photo.png\""), std::string::npos);
  EXPECT_NE(xml.find("source=\"images/foo_2.png\""), std::string::npos);
}

PAGX_TEST(PAGXExporterTest, ToDataAvoidsWindowsDeviceEntryNames) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  AddImage(doc.get(), "CON", "", MakeImageData(0xE1));
  AddImage(doc.get(), "nul", "", MakeImageData(0xE2));
  AddImage(doc.get(), "COM1", "", MakeImageData(0xE3));
  AddImage(doc.get(), "lpt9", "", MakeImageData(0xE4));
  // Only COM1-COM9 and LPT1-LPT9 are device names, not names that merely start with them.
  AddImage(doc.get(), "CON2", "", MakeImageData(0xE5));

  auto zip = pagx::PAGXExporter::ToData(*doc);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_EQ(entries.size(), 6u);

  EXPECT_EQ(entries.find("images/CON.png"), entries.end());
  EXPECT_EQ(entries.find("images/nul.png"), entries.end());
  EXPECT_EQ(entries.find("images/COM1.png"), entries.end());
  EXPECT_EQ(entries.find("images/lpt9.png"), entries.end());
  EXPECT_EQ(entries["images/img_CON.png"], BytesOf(MakeImageData(0xE1)));
  EXPECT_EQ(entries["images/img_nul.png"], BytesOf(MakeImageData(0xE2)));
  EXPECT_EQ(entries["images/img_COM1.png"], BytesOf(MakeImageData(0xE3)));
  EXPECT_EQ(entries["images/img_lpt9.png"], BytesOf(MakeImageData(0xE4)));
  EXPECT_EQ(entries["images/CON2.png"], BytesOf(MakeImageData(0xE5)));
}

PAGX_TEST(PAGXExporterTest, ToDataLimitsEntryNameLength) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  std::string longId(300, 'a');
  AddImage(doc.get(), longId, "", MakeImageData(0xC1));
  std::string otherLongId = longId;
  otherLongId[299] = 'b';
  AddImage(doc.get(), otherLongId, "", MakeImageData(0xC2));

  auto zip = pagx::PAGXExporter::ToData(*doc);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_EQ(entries.size(), 3u);
  std::string truncatedStem(128, 'a');
  ASSERT_NE(entries.find("images/" + truncatedStem + ".png"), entries.end());
  ASSERT_NE(entries.find("images/" + truncatedStem + "_2.png"), entries.end());
  EXPECT_EQ(entries["images/" + truncatedStem + ".png"], BytesOf(MakeImageData(0xC1)));
  EXPECT_EQ(entries["images/" + truncatedStem + "_2.png"], BytesOf(MakeImageData(0xC2)));
  // Every name fits the 255-byte component limit most filesystems enforce.
  for (const auto& entry : entries) {
    auto separator = entry.first.find_last_of('/');
    auto fileNameLength =
        separator == std::string::npos ? entry.first.size() : entry.first.size() - separator - 1;
    EXPECT_LT(fileNameLength, 255u) << entry.first;
  }
}

PAGX_TEST(PAGXExporterTest, ToDataReportsExternalReferences) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  AddImage(doc.get(), "img_hash", "hash:0123456789abcdef");
  AddImage(doc.get(), "img_url", "https://example.com/photo.png");
  AddImage(doc.get(), "img_local", "/Users/test/images/photo.png");

  std::string error;
  auto zip = pagx::PAGXExporter::ToData(*doc, {}, &error);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_EQ(entries.size(), 1u);
  ASSERT_NE(entries.find("document.pagx"), entries.end());
  const auto& xml = entries["document.pagx"];
  EXPECT_NE(xml.find("source=\"hash:0123456789abcdef\""), std::string::npos);
  EXPECT_NE(xml.find("source=\"https://example.com/photo.png\""), std::string::npos);
  EXPECT_NE(xml.find("source=\"/Users/test/images/photo.png\""), std::string::npos);
  EXPECT_NE(error.find("not self-contained"), std::string::npos);
  EXPECT_NE(error.find("3 image reference(s)"), std::string::npos);
  EXPECT_NE(error.find("host-supplied hash reference"), std::string::npos);
  EXPECT_NE(error.find("remote URL"), std::string::npos);
  EXPECT_NE(error.find("local file path"), std::string::npos);
}

PAGX_TEST(PAGXExporterTest, ToDataPacksBase64DataUriFilePath) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* image = AddImage(doc.get(), "img_uri", pagx::ImageDataUri(kRedPng, sizeof(kRedPng)));
  AddPatternLayer(doc.get(), image);

  std::string error;
  auto zip = pagx::PAGXExporter::ToData(*doc, {}, &error);
  ASSERT_NE(zip, nullptr);
  EXPECT_TRUE(error.empty());
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_NE(entries.find("images/img_uri.png"), entries.end());
  const auto& xml = entries["document.pagx"];
  EXPECT_NE(xml.find("source=\"images/img_uri.png\""), std::string::npos);
  EXPECT_EQ(xml.find("base64"), std::string::npos);
  EXPECT_EQ(entries["images/img_uri.png"],
            std::string(reinterpret_cast<const char*>(kRedPng), sizeof(kRedPng)));
}

PAGX_TEST(PAGXExporterTest, ToDataKeepsAbsoluteReferenceSuffixReserved) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  AddImage(doc.get(), "img_local", "/Users/test/images/photo.png");
  AddImage(doc.get(), "photo", "", MakeImageData());
  AddImage(doc.get(), "img_windows", "C:\\Users\\test\\images\\win_photo.png");
  AddImage(doc.get(), "win_photo", "", MakeImageData());

  std::string error;
  auto zip = pagx::PAGXExporter::ToData(*doc, {}, &error);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  EXPECT_EQ(entries.find("images/photo.png"), entries.end());
  EXPECT_NE(entries.find("images/photo_2.png"), entries.end());
  EXPECT_EQ(entries.find("images/win_photo.png"), entries.end());
  EXPECT_NE(entries.find("images/win_photo_2.png"), entries.end());
}

PAGX_TEST(PAGXExporterTest, ToDataUsesSniffedFormatExtensions) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  AddImage(doc.get(), "img_png", "", MakeImageData());
  AddImage(doc.get(), "img_jpeg", "", MakeBytesData(kJpegBytes, sizeof(kJpegBytes)));
  AddImage(doc.get(), "img_webp", "", MakeBytesData(kWebpBytes, sizeof(kWebpBytes)));
  AddImage(doc.get(), "img_gif", "", MakeBytesData(kGifBytes, sizeof(kGifBytes)));
  AddImage(doc.get(), "img_avif", "", MakeBytesData(kAvifBytes, sizeof(kAvifBytes)));
  AddImage(doc.get(), "img_svg", "", MakeBytesData(kSvgBytes, sizeof(kSvgBytes)));
  AddImage(doc.get(), "img_unknown", "", MakeBytesData(kGifBytes, 3));

  auto xml = pagx::PAGXExporter::ToXML(*doc);
  EXPECT_NE(xml.find("source=\"data:image/png;base64,"), std::string::npos);
  EXPECT_NE(xml.find("source=\"data:image/jpeg;base64,"), std::string::npos);
  EXPECT_NE(xml.find("source=\"data:image/webp;base64,"), std::string::npos);
  EXPECT_NE(xml.find("source=\"data:image/gif;base64,"), std::string::npos);
  EXPECT_NE(xml.find("source=\"data:image/avif;base64,"), std::string::npos);
  EXPECT_NE(xml.find("source=\"data:image/svg+xml;base64,"), std::string::npos);
  EXPECT_NE(xml.find("source=\"data:application/octet-stream;base64,"), std::string::npos);

  std::string error;
  auto zip = pagx::PAGXExporter::ToData(*doc, {}, &error);
  ASSERT_NE(zip, nullptr);
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_NE(entries.find("images/img_png.png"), entries.end());
  ASSERT_NE(entries.find("images/img_jpeg.jpeg"), entries.end());
  ASSERT_NE(entries.find("images/img_webp.webp"), entries.end());
  ASSERT_NE(entries.find("images/img_gif.gif"), entries.end());
  ASSERT_NE(entries.find("images/img_avif.avif"), entries.end());
  ASSERT_NE(entries.find("images/img_svg.svg"), entries.end());
  ASSERT_NE(entries.find("images/img_unknown.bin"), entries.end());
  EXPECT_EQ(entries["images/img_jpeg.jpeg"],
            BytesOf(MakeBytesData(kJpegBytes, sizeof(kJpegBytes))));
  EXPECT_EQ(entries["images/img_gif.gif"], BytesOf(MakeBytesData(kGifBytes, sizeof(kGifBytes))));
  EXPECT_NE(error.find("image/avif"), std::string::npos);
  EXPECT_NE(error.find("image/svg+xml"), std::string::npos);
  EXPECT_NE(error.find("unrecognized format"), std::string::npos);
}

PAGX_TEST(PAGXExporterTest, ToDataSkipsEmojiUsedOnlyBySkippedGlyphs) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto* emoji = doc->makeNode<pagx::Image>();
  emoji->filePath = "emoji:wave";
  emoji->data = MakeImageData();
  auto* font = doc->makeNode<pagx::Font>("font_1");
  auto* glyph = doc->makeNode<pagx::Glyph>();
  glyph->image = emoji;
  glyph->advance = 100;
  font->glyphs.push_back(glyph);

  pagx::PAGXExporter::Options fullOptions;
  std::string error;
  auto full = pagx::PAGXExporter::ToData(*doc, fullOptions, &error);
  ASSERT_NE(full, nullptr);
  EXPECT_TRUE(error.empty());
  std::unordered_map<std::string, std::string> fullEntries;
  ASSERT_TRUE(ReadZip(full, &fullEntries));
  EXPECT_NE(fullEntries.find("images/emoji_wave.png"), fullEntries.end());

  pagx::PAGXExporter::Options skipOptions;
  skipOptions.skipGlyphData = true;
  auto skipped = pagx::PAGXExporter::ToData(*doc, skipOptions, &error);
  ASSERT_NE(skipped, nullptr);
  EXPECT_TRUE(error.empty());
  std::unordered_map<std::string, std::string> skippedEntries;
  ASSERT_TRUE(ReadZip(skipped, &skippedEntries));
  ASSERT_EQ(skippedEntries.size(), 1u);
  EXPECT_NE(skippedEntries.find("document.pagx"), skippedEntries.end());
  EXPECT_EQ(skippedEntries["document.pagx"].find("<Font"), std::string::npos);
}

PAGX_TEST(PAGXExporterTest, ToDataWritesOnlyDocumentForEmptyInput) {
  auto empty = pagx::PAGXDocument::Make(10, 10);
  std::string error;
  auto zip = pagx::PAGXExporter::ToData(*empty, {}, &error);
  ASSERT_NE(zip, nullptr);
  EXPECT_TRUE(error.empty());
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_EQ(entries.size(), 1u);
  ASSERT_NE(entries.find("document.pagx"), entries.end());
  EXPECT_EQ(entries["document.pagx"], pagx::PAGXExporter::ToXML(*empty));

  auto hidden = pagx::PAGXDocument::Make(10, 10);
  auto* layer = hidden->makeNode<pagx::Layer>();
  layer->visible = false;
  hidden->layers.push_back(layer);
  auto hiddenZip = pagx::PAGXExporter::ToData(*hidden);
  ASSERT_NE(hiddenZip, nullptr);
  std::unordered_map<std::string, std::string> hiddenEntries;
  ASSERT_TRUE(ReadZip(hiddenZip, &hiddenEntries));
  EXPECT_NE(hiddenEntries["document.pagx"].find("<Layer"), std::string::npos);
  EXPECT_NE(hiddenEntries["document.pagx"].find("visible=\"false\""), std::string::npos);
}

PAGX_TEST(PAGXExporterTest, ToDataRoundTripsThroughDisk) {
  auto doc = pagx::PAGXDocument::Make(64, 64);
  auto* image = AddImage(doc.get(), "img_round", "", MakeImageData());
  AddPatternLayer(doc.get(), image, 64.0f);

  std::string zipError;
  auto zip = pagx::PAGXExporter::ToData(*doc, {}, &zipError);
  ASSERT_NE(zip, nullptr);
  EXPECT_TRUE(zipError.empty());
  std::unordered_map<std::string, std::string> entries;
  ASSERT_TRUE(ReadZip(zip, &entries));
  ASSERT_NE(entries.find("document.pagx"), entries.end());
  ASSERT_NE(entries.find("images/img_round.png"), entries.end());

  // A per-run directory keeps parallel or interrupted runs from deleting each other's files.
  auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  auto directory = std::filesystem::temp_directory_path() / ("pagx-exporter-roundtrip-" + suffix);
  std::error_code fileError;
  std::filesystem::remove_all(directory, fileError);
  ASSERT_TRUE(WriteEntriesToDisk(entries, directory));

  auto reloaded = pagx::PAGXImporter::FromFile((directory / "document.pagx").string());
  ASSERT_NE(reloaded, nullptr);
  auto* reloadedImage = reloaded->findNode<pagx::Image>("img_round");
  ASSERT_NE(reloadedImage, nullptr);
  auto imagePath = directory / "images" / "img_round.png";
  // The importer builds the path by string concatenation, so compare normalized paths instead of
  // the raw separators, which differ on Windows.
  EXPECT_EQ(std::filesystem::path(reloadedImage->filePath).lexically_normal(),
            imagePath.lexically_normal());
  // Reading and decoding from disk proves the archive is usable from a working directory other
  // than the extraction directory, not just that the XML mentions a path.
  EXPECT_EQ(ReadFileBytes(imagePath), entries["images/img_round.png"]);
  auto codec = tgfx::ImageCodec::MakeFrom(imagePath.string());
  ASSERT_NE(codec, nullptr);
  EXPECT_EQ(codec->width(), 8);
  EXPECT_EQ(codec->height(), 8);
  EXPECT_GT(CountOpaquePixels(reloaded, 64), 0u);

  std::filesystem::remove_all(directory, fileError);
}

PAGX_TEST(PAGXExporterTest, ToDataDoesNotModifyDocumentAndIsDeterministic) {
  auto doc = pagx::PAGXDocument::Make(100, 100);
  auto imageData = MakeImageData();
  auto* image = AddImage(doc.get(), "img_alpha", "hash:abc", imageData);
  AddPatternLayer(doc.get(), image);
  auto xmlBefore = pagx::PAGXExporter::ToXML(*doc);

  std::string error;
  auto first = pagx::PAGXExporter::ToData(*doc, {}, &error);
  ASSERT_NE(first, nullptr);
  EXPECT_TRUE(error.empty());
  auto second = pagx::PAGXExporter::ToData(*doc, {}, &error);
  ASSERT_NE(second, nullptr);
  EXPECT_TRUE(error.empty());
  EXPECT_EQ(BytesOf(first), BytesOf(second));
  std::unordered_map<std::string, std::string> firstEntries;
  ASSERT_TRUE(ReadZip(first, &firstEntries));

  EXPECT_EQ(image->id, "img_alpha");
  EXPECT_EQ(image->filePath, "hash:abc");
  EXPECT_EQ(image->data, imageData);
  EXPECT_EQ(pagx::PAGXExporter::ToXML(*doc), xmlBefore);
  // Data wins over filePath inside an archive, while ToXML keeps the original path priority.
  EXPECT_NE(firstEntries["document.pagx"].find("source=\"images/img_alpha.png\""),
            std::string::npos);
  EXPECT_NE(xmlBefore.find("source=\"hash:abc\""), std::string::npos);
}

}  // namespace pag
