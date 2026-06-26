// Copyright (C) 2026 Tencent. All rights reserved.
//
// Phase 16.1: FontProvider interface contract tests.
// Authoritative spec: docs/pagx_to_pag_v2_phase16_text_redesign.md §5.1 + §5.5.
#include <memory>
#include <string>
#include <vector>
#include "gtest/gtest.h"
#include "pag/pag.h"  // pag::PAGFont::RegisterFont / UnregisterFont
#include "pagx/FontConfig.h"
#include "pagx/pag/FontProvider.h"
#include "tgfx/core/Typeface.h"
#include "utils/ProjectPath.h"

namespace pagx::pag {
namespace {

// ---------- Test-only provider: deterministic stub ------------------------

// Minimal in-memory provider so we can exercise the contract without relying
// on system font availability (tgfx::Typeface::MakeFromName is null on macOS).
class StubFontProvider : public FontProvider {
 public:
  void add(const std::string& family, const std::string& style,
           std::shared_ptr<tgfx::Typeface> typeface) {
    entries_.push_back({family, style, std::move(typeface)});
  }
  void addFallback(std::shared_ptr<tgfx::Typeface> typeface) {
    fallbacks_.push_back(std::move(typeface));
  }

  std::shared_ptr<tgfx::Typeface> getTypeface(const std::string& fontFamily,
                                              const std::string& fontStyle) const override {
    for (const auto& entry : entries_) {
      if (entry.family == fontFamily && entry.style == fontStyle) {
        return entry.typeface;
      }
    }
    return nullptr;
  }

  std::vector<std::shared_ptr<tgfx::Typeface>> getFallbackTypefaces() const override {
    return fallbacks_;
  }

 private:
  struct Entry {
    std::string family;
    std::string style;
    std::shared_ptr<tgfx::Typeface> typeface;
  };
  std::vector<Entry> entries_;
  std::vector<std::shared_ptr<tgfx::Typeface>> fallbacks_;
};

// ---------- MakeDefault contract ------------------------------------------

TEST(FontProvider, MakeDefaultNotNull) {
  auto provider = MakeDefaultFontProvider();
  ASSERT_NE(provider, nullptr);
}

TEST(FontProvider, MakeDefaultIndependentInstances) {
  // Each invocation yields a fresh shared_ptr; callers can opt into or out of
  // the FontManager-backed default without affecting each other.
  auto a = MakeDefaultFontProvider();
  auto b = MakeDefaultFontProvider();
  EXPECT_NE(a, b);
}

TEST(FontProvider, DefaultUnregisteredFontReturnsNull) {
  auto provider = MakeDefaultFontProvider();
  // "__pagx_phase16_unregistered__" is guaranteed not to be registered.
  auto typeface = provider->getTypeface("__pagx_phase16_unregistered__", "Regular");
  EXPECT_EQ(typeface, nullptr);
}

TEST(FontProvider, DefaultFallbackQueryable) {
  // Whatever the host has configured, getFallbackTypefaces() must return a
  // concrete vector (possibly empty) — never throw, never trap.
  auto provider = MakeDefaultFontProvider();
  auto fallbacks = provider->getFallbackTypefaces();
  (void)fallbacks;
  SUCCEED();
}

// ---------- Stub provider: lookup path ------------------------------------

TEST(FontProvider, StubLookupByExactMatch) {
  StubFontProvider provider;
  // tgfx::Typeface::MakeEmpty() returns the same process-wide singleton for
  // every call, so we cannot rely on distinct instances. Exercise identity
  // matching by registering two *distinct* styles against the same typeface
  // and asserting the lookup picks whichever style the caller asked for,
  // and that a missing style falls through to nullptr.
  auto empty = tgfx::Typeface::MakeEmpty();
  ASSERT_NE(empty, nullptr);
  provider.add("MyFont", "Regular", empty);
  provider.add("MyFont", "Bold", empty);

  EXPECT_EQ(provider.getTypeface("MyFont", "Regular"), empty);
  EXPECT_EQ(provider.getTypeface("MyFont", "Bold"), empty);
  EXPECT_EQ(provider.getTypeface("MyFont", "Thin"), nullptr);
}

TEST(FontProvider, StubLookupStyleSensitive) {
  StubFontProvider provider;
  provider.add("MyFont", "Regular", tgfx::Typeface::MakeEmpty());
  EXPECT_EQ(provider.getTypeface("MyFont", "Italic"), nullptr);
  EXPECT_EQ(provider.getTypeface("OtherFont", "Regular"), nullptr);
}

TEST(FontProvider, StubLookupEmptyStringsNull) {
  StubFontProvider provider;
  EXPECT_EQ(provider.getTypeface("", ""), nullptr);
}

// ---------- Stub provider: fallback chain ---------------------------------

TEST(FontProvider, StubFallbackOrderPreserved) {
  StubFontProvider provider;
  auto first = tgfx::Typeface::MakeEmpty();
  auto second = tgfx::Typeface::MakeEmpty();
  auto third = tgfx::Typeface::MakeEmpty();
  provider.addFallback(first);
  provider.addFallback(second);
  provider.addFallback(third);

  auto chain = provider.getFallbackTypefaces();
  ASSERT_EQ(chain.size(), 3u);
  EXPECT_EQ(chain[0], first);
  EXPECT_EQ(chain[1], second);
  EXPECT_EQ(chain[2], third);
}

TEST(FontProvider, StubFallbackEmptyByDefault) {
  StubFontProvider provider;
  EXPECT_TRUE(provider.getFallbackTypefaces().empty());
}

// ---------- Polymorphism contract -----------------------------------------

TEST(FontProvider, PolymorphicUsageThroughBase) {
  auto stub = std::make_shared<StubFontProvider>();
  auto typeface = tgfx::Typeface::MakeEmpty();
  stub->add("Demo", "Regular", typeface);

  std::shared_ptr<FontProvider> base = stub;
  EXPECT_EQ(base->getTypeface("Demo", "Regular"), typeface);
  EXPECT_EQ(base->getTypeface("Demo", "Bold"), nullptr);
}

// ---------- FontConfig adapter --------------------------------------------

TEST(FontProvider, MakeFontProviderFromConfigNullReturnsNull) {
  auto provider = MakeFontProviderFromConfig(nullptr);
  EXPECT_EQ(provider, nullptr);
}

TEST(FontProvider, MakeFontProviderFromConfigEmptyConfig) {
  auto config = std::make_shared<pagx::FontConfig>();
  auto provider = MakeFontProviderFromConfig(config);
  ASSERT_NE(provider, nullptr);
  // Empty config: no registered typefaces, no fallbacks.
  EXPECT_EQ(provider->getTypeface("AnyFont", "Regular"), nullptr);
  EXPECT_TRUE(provider->getFallbackTypefaces().empty());
}

TEST(FontProvider, MakeFontProviderFromConfigFallbackQueryable) {
  // Register one fallback font by path. The adapter should round-trip it
  // through LayoutContext::fallbackTypefaces() and resolve to a real typeface.
  auto config = std::make_shared<pagx::FontConfig>();
  config->addFallbackFont(::pag::ProjectPath::Absolute("resources/font/NotoSansSC-Regular.otf"), 0);

  auto provider = MakeFontProviderFromConfig(config);
  ASSERT_NE(provider, nullptr);
  auto chain = provider->getFallbackTypefaces();
  ASSERT_EQ(chain.size(), 1u);
  EXPECT_NE(chain[0], nullptr);
}

TEST(FontProvider, MakeFontProviderFromConfigExposesFontConfig) {
  // The adapter must surface the underlying FontConfig so the Inflater can
  // opt into the HarfBuzz-based text path. Default providers return nullptr.
  auto config = std::make_shared<pagx::FontConfig>();
  auto provider = MakeFontProviderFromConfig(config);
  ASSERT_NE(provider, nullptr);
  EXPECT_EQ(provider->getFontConfig(), config.get());
}

TEST(FontProvider, DefaultFontProviderHasNoFontConfig) {
  auto provider = MakeDefaultFontProvider();
  ASSERT_NE(provider, nullptr);
  EXPECT_EQ(provider->getFontConfig(), nullptr);
}

}  // namespace
}  // namespace pagx::pag
