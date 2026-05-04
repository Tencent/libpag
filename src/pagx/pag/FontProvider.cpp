// Copyright (C) 2026 Tencent. All rights reserved.
#include "pagx/pag/FontProvider.h"
#include "pagx/FontConfig.h"
#include "rendering/FontManager.h"

namespace pagx::pag {

namespace {

// Default implementation: adapts pag::FontManager to the FontProvider
// contract. Host applications typically register fonts via
// pag::PAGFont::RegisterFont during startup; the Inflater then consults
// this adapter for every text element.
class DefaultFontProvider : public FontProvider {
 public:
  std::shared_ptr<tgfx::Typeface> getTypeface(const std::string& fontFamily,
                                              const std::string& fontStyle) const override {
    return ::pag::FontManager::GetTypefaceWithoutFallback(fontFamily, fontStyle);
  }

  std::vector<std::shared_ptr<tgfx::Typeface>> getFallbackTypefaces() const override {
    auto holders = ::pag::FontManager::GetFallbackTypefaces();
    std::vector<std::shared_ptr<tgfx::Typeface>> out;
    out.reserve(holders.size());
    for (auto& holder : holders) {
      if (holder == nullptr) {
        continue;
      }
      auto typeface = holder->getTypeface();
      if (typeface != nullptr) {
        out.push_back(std::move(typeface));
      }
    }
    return out;
  }
};

// Wraps a pagx::FontConfig so PAG v2 Inflater resolves typefaces against
// the same registry the PAGX layout engine uses. Holds a non-const
// shared_ptr because FontConfig's lookup API is non-const (lazy-load of
// deferred fallback fonts). The const virtuals forward to FontConfig via a
// mutable pointer — lazy-load is an invisible optimization so callers still
// see a pure read-only surface.
class FontConfigFontProvider : public FontProvider {
 public:
  explicit FontConfigFontProvider(std::shared_ptr<pagx::FontConfig> config)
      : fontConfig(std::move(config)) {
  }

  std::shared_ptr<tgfx::Typeface> getTypeface(const std::string& fontFamily,
                                              const std::string& fontStyle) const override {
    if (fontConfig == nullptr) {
      return nullptr;
    }
    return fontConfig->findTypeface(fontFamily, fontStyle);
  }

  std::vector<std::shared_ptr<tgfx::Typeface>> getFallbackTypefaces() const override {
    if (fontConfig == nullptr) {
      return {};
    }
    return fontConfig->fallbackTypefaces();
  }

 private:
  std::shared_ptr<pagx::FontConfig> fontConfig;
};

}  // namespace

std::shared_ptr<FontProvider> MakeDefaultFontProvider() {
  return std::make_shared<DefaultFontProvider>();
}

std::shared_ptr<FontProvider> MakeFontProviderFromConfig(
    std::shared_ptr<pagx::FontConfig> fontConfig) {
  if (fontConfig == nullptr) {
    return nullptr;
  }
  return std::make_shared<FontConfigFontProvider>(std::move(fontConfig));
}

}  // namespace pagx::pag
