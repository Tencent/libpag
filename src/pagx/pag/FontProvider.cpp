// Copyright (C) 2026 Tencent. All rights reserved.
#include "pagx/pag/FontProvider.h"
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

}  // namespace

std::shared_ptr<FontProvider> MakeDefaultFontProvider() {
  return std::make_shared<DefaultFontProvider>();
}

}  // namespace pagx::pag
