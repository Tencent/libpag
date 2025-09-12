/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#ifdef PAG_USE_HARFBUZZ

#include "TextShaperHarfbuzz.h"
#include <list>
#include <map>
#include "base/utils/Log.h"
#include "hb.h"
#include "rendering/FontManager.h"

namespace pag {
template <typename T>
struct PtrWrapper {
  explicit PtrWrapper(std::shared_ptr<T> ptr) : ptr(std::move(ptr)) {
  }
  std::shared_ptr<T> ptr;
};

hb_blob_t* HBGetTable(hb_face_t*, hb_tag_t tag, void* userData) {
  auto data = reinterpret_cast<PtrWrapper<tgfx::Typeface>*>(userData)->ptr->copyTableData(tag);
  if (data == nullptr) {
    return nullptr;
  }
  auto wrapper = new PtrWrapper<tgfx::Data>(data);
  return hb_blob_create(static_cast<const char*>(data->data()),
                        static_cast<unsigned int>(data->size()), HB_MEMORY_MODE_READONLY,
                        static_cast<void*>(wrapper),
                        [](void* ctx) { delete reinterpret_cast<PtrWrapper<tgfx::Data>*>(ctx); });
}

std::shared_ptr<hb_face_t> CreateHBFace(std::shared_ptr<tgfx::Typeface> typeface) {
  if (typeface == nullptr) {
    return nullptr;
  }
  std::shared_ptr<hb_face_t> hbFace;
  auto wrapper = new PtrWrapper<tgfx::Typeface>(typeface);
  hbFace = std::shared_ptr<hb_face_t>(
      hb_face_create_for_tables(
          HBGetTable, static_cast<void*>(wrapper),
          [](void* ctx) { delete reinterpret_cast<PtrWrapper<tgfx::Typeface>*>(ctx); }),
      hb_face_destroy);
  if (hb_face_get_empty() == hbFace.get()) {
    return nullptr;
  }
  hb_face_set_index(hbFace.get(), 0);
  if (hbFace == nullptr) {
    return nullptr;
  }
  hb_face_set_upem(hbFace.get(), typeface->unitsPerEm());
  return hbFace;
}

class HBLockedFontCache {
 public:
  HBLockedFontCache(std::list<uint32_t>* lru, std::map<uint32_t, std::shared_ptr<hb_font_t>>* cache,
                    std::mutex* mutex)
      : lru(lru), cache(cache), mutex(mutex) {
    mutex->lock();
  }
  HBLockedFontCache(const HBLockedFontCache&) = delete;
  HBLockedFontCache& operator=(const HBLockedFontCache&) = delete;
  HBLockedFontCache& operator=(HBLockedFontCache&&) = delete;

  ~HBLockedFontCache() {
    mutex->unlock();
  }

  std::shared_ptr<hb_font_t> find(uint32_t fontId) {
    auto iter = std::find(lru->begin(), lru->end(), fontId);
    if (iter == lru->end()) {
      return nullptr;
    }
    lru->erase(iter);
    lru->push_front(fontId);
    return (*cache)[fontId];
  }
  std::shared_ptr<hb_font_t> insert(uint32_t fontId, std::shared_ptr<hb_font_t> hbFont) {
    if (hb_font_get_empty() == hbFont.get()) {
      return nullptr;
    }
    static const int MaxCacheSize = 100;
    cache->insert(std::make_pair(fontId, std::move(hbFont)));
    lru->push_front(fontId);
    while (lru->size() > MaxCacheSize) {
      auto id = lru->back();
      lru->pop_back();
      cache->erase(id);
    }
    return (*cache)[fontId];
  }
  void reset() {
    lru->clear();
    cache->clear();
  }

 private:
  std::list<uint32_t>* lru;
  std::map<uint32_t, std::shared_ptr<hb_font_t>>* cache;
  std::mutex* mutex;
};

static HBLockedFontCache GetHBFontCache() {
  static auto* HBFontCacheMutex = new std::mutex();
  static auto* HBFontLRU = new std::list<uint32_t>();
  static auto* HBFontCache = new std::map<uint32_t, std::shared_ptr<hb_font_t>>();
  return {HBFontLRU, HBFontCache, HBFontCacheMutex};
}

static std::shared_ptr<hb_font_t> CreateHBFont(std::shared_ptr<tgfx::Typeface> typeface) {
  if (typeface == nullptr) {
    return nullptr;
  }
  auto cache = GetHBFontCache();
  auto hbFont = cache.find(typeface->uniqueID());
  if (hbFont == nullptr) {
    auto hbFace = CreateHBFace(typeface);
    if (hbFace == nullptr) {
      return nullptr;
    }
    hbFont = cache.insert(typeface->uniqueID(), std::shared_ptr<hb_font_t>(
                                                    hb_font_create(hbFace.get()), hb_font_destroy));
  }
  return hbFont;
}

static std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> Shape(
    const std::string& text, std::shared_ptr<tgfx::Typeface> typeface) {
  auto hbFont = CreateHBFont(typeface);
  if (hbFont == nullptr) {
    return {};
  }

  auto hbBuffer = std::shared_ptr<hb_buffer_t>(hb_buffer_create(), hb_buffer_destroy);
  if (!hb_buffer_allocation_successful(hbBuffer.get())) {
    LOGI("TextShaperHarfbuzz::shape text = %s, alloc harfbuzz(%p) failure", text.c_str(),
         hbBuffer.get());
    return {};
  }
  hb_buffer_add_utf8(hbBuffer.get(), text.data(), -1, 0, -1);
  hb_buffer_guess_segment_properties(hbBuffer.get());
  hb_shape(hbFont.get(), hbBuffer.get(), nullptr, 0);
  unsigned count = 0;
  auto* infos = hb_buffer_get_glyph_infos(hbBuffer.get(), &count);
  std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> result;
  for (unsigned i = 0; i < count; ++i) {
    auto length = (i + 1 == count ? text.length() : infos[i + 1].cluster) - infos[i].cluster;
    if (length == 0) {
      continue;
    }
    result.emplace_back(infos[i].codepoint, infos[i].cluster, length);
  }
  return result;
}

struct HBGlyph {
  std::string text;
  tgfx::GlyphID glyphID = 0;
  uint32_t stringIndex = 0;
  std::shared_ptr<tgfx::Typeface> typeface;
};

static bool Shape(std::list<HBGlyph>& glyphs, std::shared_ptr<tgfx::Typeface> typeface) {
  bool allShaped = true;
  for (auto iter = glyphs.begin(); iter != glyphs.end(); ++iter) {
    if (iter->glyphID == 0) {
      auto string = iter->text;
      auto infos = ::pag::Shape(string, typeface);
      if (infos.empty()) {
        allShaped = false;
      } else {
        auto stringIndex = iter->stringIndex;
        for (size_t i = 0; i < infos.size(); ++i) {
          auto [codepoint, cluster, length] = infos[i];
          HBGlyph glyph;
          glyph.text = string.substr(cluster, length);
          glyph.stringIndex = stringIndex + cluster;
          if (codepoint != 0) {
            glyph.glyphID = codepoint;
            glyph.typeface = typeface;
          } else {
            allShaped = false;
          }
          if (i == 0) {
            iter = glyphs.erase(iter);
            iter = glyphs.insert(iter, glyph);
          } else {
            iter = glyphs.insert(++iter, glyph);
          }
        }
      }
    }
  }
  return allShaped;
}

std::vector<ShapedGlyph> TextShaperHarfbuzz::Shape(const std::string& text,
                                                   std::shared_ptr<tgfx::Typeface> face) {
  std::list<HBGlyph> hbGlyphs = {};
  hbGlyphs.emplace_back(HBGlyph{text, {}, 0, nullptr});
  bool allShaped = false;
  if (face && !face->fontFamily().empty()) {
    allShaped = ::pag::Shape(hbGlyphs, std::move(face));
  }
  if (!allShaped) {
    auto typefaces = FontManager::GetFallbackTypefaces();
    for (const auto& faceHolder : typefaces) {
      auto typeface = faceHolder->getTypeface();
      if (typeface && ::pag::Shape(hbGlyphs, std::move(typeface))) {
        break;
      }
    }
  }
  std::vector<ShapedGlyph> glyphs = {};
  for (const auto& hbGlyph : hbGlyphs) {
    glyphs.emplace_back(hbGlyph.typeface, hbGlyph.glyphID, hbGlyph.stringIndex);
  }
  return glyphs;
}

void TextShaperHarfbuzz::PurgeCaches() {
  auto cache = GetHBFontCache();
  cache.reset();
}
}  // namespace pag

#endif
