// Copyright (C) 2026 Tencent. All rights reserved.
#pragma once

// IMPORTANT: This header depends on tgfx::Typeface (a tgfx core data type,
// not a rendering-layer type). PAGLoader.h is the only other public header
// that pulls in tgfx; include FontProvider.h only from modules that also
// consume PAGLoader.h (i.e. rendering callers).

#include <memory>
#include <string>
#include <vector>
#include "tgfx/core/Typeface.h"

namespace pagx {
class FontConfig;
}  // namespace pagx

namespace pagx::pag {

/**
 * Resolves typeface lookups for PAG v2 Inflater (Phase 16 runtime-shape mode).
 *
 * Why injectable: tgfx::Typeface::MakeFromName returns null on macOS / Linux /
 * Android (only Windows has the DirectWrite backend). PAG v2 relies on
 * runtime-registered typefaces, and different hosts register through different
 * mechanisms — PAGFont in libpag v1, a bundled asset manager in iOS/Android
 * SDK, a custom adapter in tools. FontProvider is the single indirection
 * layer the Inflater consults; callers wire the host-specific backend in.
 *
 * Thread-safety: Implementations MUST be safe to invoke from multiple
 * Inflater threads concurrently and MUST treat each call as a read-only
 * lookup. The Inflater may cache the returned typeface for the duration of
 * a single Inflate() call but does not keep long-lived references.
 */
class FontProvider {
 public:
  virtual ~FontProvider() = default;

  /**
   * Look up a typeface by family + style. Returns nullptr when no typeface
   * is registered for the requested pair; the Inflater then walks
   * getFallbackTypefaces() before emitting InflateFontCreateFailed.
   *
   * `fontFamily` / `fontStyle` are the exact strings serialized in
   * ElementTextData (Phase 16 mirrors v1 pag::TextDocument). Empty strings
   * signal "no preference"; implementations may return nullptr or a
   * best-effort default.
   */
  virtual std::shared_ptr<tgfx::Typeface> getTypeface(
      const std::string& fontFamily, const std::string& fontStyle) const = 0;

  /**
   * Ordered fallback typefaces. The Inflater walks this list in order; the
   * first typeface whose shaper produces a non-empty glyph run wins.
   * Returning an empty vector is legal — the caller will degrade to
   * InflateFontCreateFailed and render the ElementText as an empty slot.
   */
  virtual std::vector<std::shared_ptr<tgfx::Typeface>> getFallbackTypefaces()
      const = 0;

  /**
   * Optional: returns the pagx::FontConfig this provider is backed by, or
   * nullptr if the provider doesn't own one. When non-null the Inflater
   * switches its text path from the tgfx primitive shaper
   * (TextBlob::MakeFrom(string, font)) to the HarfBuzz-based pagx::TextShaper,
   * matching the shaper PAGX layout uses and eliminating the per-glyph
   * advance drift between Path A (LayerBuilder) and Path B (Inflater).
   *
   * Default returns nullptr so existing FontProvider implementations keep
   * working on the primitive path without modification. Host adapters that
   * already own a FontConfig (e.g. MakeFontProviderFromConfig) override this
   * to expose it.
   *
   * Returns a raw pointer rather than shared_ptr to keep the FontProvider
   * header independent of pagx/FontConfig.h; the adapter is responsible for
   * keeping the FontConfig alive for the duration of the Inflate() call.
   */
  virtual pagx::FontConfig* getFontConfig() const {
    return nullptr;
  }
};

/**
 * Default FontProvider: wraps the process-global pag::FontManager.
 *
 * Resolution rules:
 *   getTypeface(family, style)  -> pag::FontManager::GetTypefaceWithoutFallback
 *   getFallbackTypefaces()      -> pag::FontManager::GetFallbackTypefaces()
 *
 * This adapter assumes callers have pre-registered typefaces through
 * pag::PAGFont::RegisterFont / SetFallbackFontPaths. On pristine hosts the
 * default provider returns nullptr for every lookup — the Inflater will
 * emit InflateFontCreateFailed warnings for every text element. Hosts that
 * need custom resolution (embedded asset bundles, app-local caches, ...)
 * should pass their own FontProvider via PAGLoader::Options::fontProvider
 * rather than relying on this default.
 */
std::shared_ptr<FontProvider> MakeDefaultFontProvider();

/**
 * Builds a FontProvider that delegates to a pagx::FontConfig — the same
 * registry the PAGX layout engine uses. Purpose: test fixtures and tools
 * that render both Path A (LayerBuilder / PAGXDocument::applyLayout) and
 * Path B (PAGLoader / Inflater) can share a single FontConfig, guaranteeing
 * the two paths resolve every fontFamily/fontStyle to the same typeface.
 *
 * getTypeface / getFallbackTypefaces delegate to LayoutContext::findTypeface /
 * LayoutContext::fallbackTypefaces (constructed over the shared FontConfig).
 * Those are non-const because they may lazy-load deferred fallback fonts; the
 * adapter keeps its shared_ptr<FontConfig> non-const and performs the lazy load
 * inside the FontProvider's const virtual methods. That's safe — FontProvider's
 * const contract is about not exposing mutable state to callers, and lazy-load
 * is an invisible optimization.
 *
 * Returns nullptr if `fontConfig` is nullptr.
 */
std::shared_ptr<FontProvider> MakeFontProviderFromConfig(
    std::shared_ptr<pagx::FontConfig> fontConfig);

}  // namespace pagx::pag
