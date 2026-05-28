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

#include "pagx/html/importer/HTMLTextFragmentBuilder.h"
#include <cmath>
#include <utility>
#include "pagx/PAGXDocument.h"
#include "pagx/html/importer/HTMLDetail.h"
#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include "pagx/html/importer/HTMLIdAllocator.h"
#include "pagx/html/importer/HTMLLayerBuilder.h"
#include "pagx/html/importer/HTMLStyleCascade.h"
#include "pagx/html/importer/HTMLValueParser.h"
#include "pagx/nodes/Fill.h"
#include "pagx/nodes/Group.h"
#include "pagx/nodes/Layer.h"
#include "pagx/nodes/Text.h"
#include "pagx/nodes/TextBox.h"
#include "pagx/xml/XMLDOM.h"

namespace pagx {

using namespace pagx::html;

HTMLTextFragmentBuilder::HTMLTextFragmentBuilder(HTMLDiagnosticSink& sink,
                                                 HTMLValueParser& valueParser,
                                                 HTMLLayerBuilder& layerBuilder,
                                                 HTMLStyleCascade& styleCascade,
                                                 HTMLIdAllocator& idAllocator)
    : _diagnostics(sink), _valueParser(valueParser), _layerBuilder(layerBuilder),
      _styleCascade(styleCascade), _idAllocator(idAllocator) {
}

void HTMLTextFragmentBuilder::bindDocument(PAGXDocument* document) {
  _document = document;
}

Text* HTMLTextFragmentBuilder::buildTextElement(const TextFragment& fragment) {
  auto t = _document->makeNode<Text>();
  t->text = fragment.text;
  t->fontFamily = fragment.fontFamily;
  t->fontStyle = fragment.fontStyleName;
  t->fontSize = fragment.fontSize;
  t->letterSpacing = fragment.letterSpacing;
  return t;
}

Fill* HTMLTextFragmentBuilder::buildTextFill(const TextFragment& fragment) {
  if (fragment.fillImage.empty()) {
    return _layerBuilder.buildSolidFill(fragment.color);
  }
  auto fill = _document->makeNode<Fill>();
  fill->color = _layerBuilder.parseGradientByValue(fragment.fillImage);
  if (!fill->color) {
    // Unparseable gradient: fall back to solid color so text stays visible. This mirrors
    // what `applyBackgroundVisuals` does for an unparseable box gradient.
    return _layerBuilder.buildSolidFill(fragment.color);
  }
  return fill;
}

HTMLTextFragmentBuilder::TextFragment HTMLTextFragmentBuilder::makeFragment(
    const HTMLInheritedStyle& inherited) {
  TextFragment frag;
  frag.fontFamily =
      inherited.primaryFontFamily.empty() ? HTML_DEFAULT_FONT_FAMILY : inherited.primaryFontFamily;
  frag.fontStyleName = inherited.fontStyleName;
  frag.fontSize = inherited.fontSizePx;
  frag.letterSpacing = inherited.letterSpacingPx;
  frag.color = inherited.resolvedTextColor;
  frag.textDecoration = inherited.textDecoration;
  frag.fillImage = inherited.textFillImage;
  // Resolve once per fragment so convertTextLeaf can derive TextBox.lineHeight without
  // re-parsing the cascade. Empty / `normal` cascades resolve to NaN, signalling "no
  // explicit contribution" — the line-box then collapses to the parent's font metrics.
  frag.lineHeight = _valueParser.resolveLineHeightPx(inherited.lineHeight, inherited.fontSizePx);
  return frag;
}

bool HTMLTextFragmentBuilder::fragmentsShareStyle(const TextFragment& a, const TextFragment& b) {
  // CSS unit conversions (em -> px etc.) introduce sub-pixel rounding that should not
  // prevent two same-styled inline runs from merging. Use a small absolute tolerance for
  // the float fields. The tolerance is well below any visually-meaningful step.
  constexpr float epsilon = 1e-3f;
  return a.fontFamily == b.fontFamily && a.fontStyleName == b.fontStyleName &&
         std::fabs(a.fontSize - b.fontSize) < epsilon &&
         std::fabs(a.letterSpacing - b.letterSpacing) < epsilon && a.color == b.color &&
         a.textDecoration == b.textDecoration && a.fillImage == b.fillImage;
}

bool HTMLTextFragmentBuilder::fragmentMatchesInherited(const TextFragment& a,
                                                       const HTMLInheritedStyle& inherited) {
  constexpr float epsilon = 1e-3f;
  const bool familyMatch = inherited.primaryFontFamily.empty()
                               ? a.fontFamily == HTML_DEFAULT_FONT_FAMILY
                               : a.fontFamily == inherited.primaryFontFamily;
  return familyMatch && a.fontStyleName == inherited.fontStyleName &&
         std::fabs(a.fontSize - inherited.fontSizePx) < epsilon &&
         std::fabs(a.letterSpacing - inherited.letterSpacingPx) < epsilon &&
         a.color == inherited.resolvedTextColor && a.textDecoration == inherited.textDecoration &&
         a.fillImage == inherited.textFillImage;
}

void HTMLTextFragmentBuilder::appendFragment(std::vector<TextFragment>& out,
                                             const HTMLInheritedStyle& inherited,
                                             std::string text) {
  if (text.empty()) return;
  // Fast path: when the new run shares style with the back of `out`, just extend its text and
  // skip the fragment construction entirely. Saves four std::string copies per merged run on
  // typical rich-text inputs. `reserve` here pre-grows the buffer to the final size so the
  // amortised string append doesn't trigger an extra reallocation when the merged tail is
  // long (e.g. multi-line CJK paragraphs whose every newline gets glued onto the same run).
  if (!out.empty() && fragmentMatchesInherited(out.back(), inherited)) {
    auto& dst = out.back().text;
    dst.reserve(dst.size() + text.size());
    dst.append(text);
    // CSS line-box height is the max of all participants' line-heights, so a later run that
    // shares glyph styling but carries a taller `line-height` (e.g. a nested span used for
    // vertical centring inside a fixed-height badge) must still raise the merged fragment's
    // value. Without this, the merged line-height freezes at the first run's resolution and
    // later inner-span overrides would be silently dropped.
    float incomingLh = _valueParser.resolveLineHeightPx(inherited.lineHeight, inherited.fontSizePx);
    if (!std::isnan(incomingLh) && incomingLh > 0 &&
        (std::isnan(out.back().lineHeight) || incomingLh > out.back().lineHeight)) {
      out.back().lineHeight = incomingLh;
    }
    return;
  }
  TextFragment frag = makeFragment(inherited);
  frag.text = std::move(text);
  out.push_back(std::move(frag));
}

void HTMLTextFragmentBuilder::collectFragments(const std::shared_ptr<DOMNode>& element,
                                               const HTMLInheritedStyle& inherited,
                                               std::vector<TextFragment>& out, int depth) {
  if (depth >= MAX_HTML_RECURSION_DEPTH) {
    _diagnostics.warn(
        "html: maximum recursion depth reached inside text leaf; remaining inline runs skipped");
    return;
  }
  auto child = element->getFirstChild();
  while (child) {
    if (child->type == DOMNodeType::Text) {
      // Map every collapsible whitespace character in the text node — newline,
      // carriage return, tab — to a regular space before handing the run to
      // `appendFragment`. CSS treats these as equivalent to spaces inside a
      // `white-space: normal | nowrap` inline-formatting-context, so the
      // downstream `CollapseHTMLWhitespace` pass (whose front-trim only drops
      // ASCII spaces, intentionally, to preserve `<br>`-originated `\n`) can
      // collapse and trim them away just like the browser would. Without this
      // step, indented source HTML such as `<h1>\n        Title</h1>` leaks
      // its source newline into the first fragment ("\n Title"), shifting the
      // rendered baseline by one space and / or introducing a phantom line
      // break. `<br>` keeps its hard `\n` because that path appends the
      // newline directly (not via a text node), so its semantics are unaffected.
      std::string text = child->name;
      for (char& c : text) {
        if (c == '\n' || c == '\r' || c == '\t') c = ' ';
      }
      appendFragment(out, inherited, std::move(text));
    } else if (child->type == DOMNodeType::Element) {
      if (child->name == "br") {
        appendFragment(out, inherited, "\n");
      } else if (IsInlineRunTag(child->name)) {
        HTMLInheritedStyle childInherited = _styleCascade.computeInherited(child, inherited);
        collectFragments(child, childInherited, out, depth + 1);
      } else {
        _diagnostics.warn("html: '<" + child->name + ">' not supported inside text leaf; skipped");
      }
    }
    child = child->getNextSibling();
  }
}

Layer* HTMLTextFragmentBuilder::convertTextLeaf(const std::shared_ptr<DOMNode>& element,
                                                const HTMLBoxAttributes& box,
                                                const HTMLInheritedStyle& inherited) {
  std::vector<TextFragment> fragments;
  collectFragments(element, inherited, fragments);
  // Collapse whitespace as if the inline-formatting-context were a single CSS run, not as
  // independent fragments. Each fragment carries its own font size, so a trailing space in a
  // parent span followed by a child span (`灵魂内核层 <span>SOUL...</span>`) must keep that
  // space at the parent's font size — trimming the trailing space per-fragment would render
  // the two runs glued together. Strategy: only the first non-empty fragment trims its
  // leading whitespace; intermediate fragments keep both ends; cross-fragment boundary
  // whitespace is collapsed by trimming a fragment's leading whitespace when the previous
  // fragment ended with whitespace; trailing whitespace at the very end of the run is removed
  // in a post-pass on the last surviving fragment.
  size_t writeIndex = 0;
  bool prevEndsWithWhitespace = false;
  for (size_t i = 0; i < fragments.size(); ++i) {
    bool trimLeading = (writeIndex == 0) || prevEndsWithWhitespace;
    fragments[i].text =
        CollapseHTMLWhitespace(fragments[i].text, trimLeading, /*trimTrailing=*/false);
    if (fragments[i].text.empty()) continue;
    char back = fragments[i].text.back();
    prevEndsWithWhitespace = (back == ' ' || back == '\n');
    if (writeIndex != i) {
      fragments[writeIndex] = std::move(fragments[i]);
    }
    ++writeIndex;
  }
  while (writeIndex > 0) {
    auto& last = fragments[writeIndex - 1].text;
    while (!last.empty() && (last.back() == ' ' || last.back() == '\n')) {
      last.pop_back();
    }
    if (last.empty()) {
      --writeIndex;
    } else {
      break;
    }
  }
  fragments.resize(writeIndex);
  if (fragments.empty()) {
    return nullptr;
  }
  if (box.absolute) {
    _diagnostics.warn("html: position: absolute on text leaf '<" + element->name +
                      ">' is downgraded to absolute on the surrounding Layer");
  }

  bool hasBgVisuals = HTMLLayerBuilder::hasBackgroundVisuals(box);
  bool hasMultipleFragments = fragments.size() > 1;
  bool hasNoWrap = !inherited.whiteSpace.empty() && ToLower(Trim(inherited.whiteSpace)) == "nowrap";
  std::string wm = ToLower(Trim(inherited.writingMode));
  bool isVertical = wm == "vertical-rl" || wm == "vertical-lr";
  // A child inline run that carries an explicit `line-height` (e.g. the digit-badge idiom
  // `<div height:20><span line-height:20>02</span></div>`) raises the line-box height
  // beyond the run's font metrics. The bare `<Text>+<Fill>` no-TextBox path has nowhere to
  // record that, so the glyph would render at its intrinsic font height aligned to the
  // top of the box instead of being vertically padded to the line-box. Force a TextBox in
  // that case so the line-height fallback below has somewhere to land.
  bool fragmentHasExplicitLineHeight = false;
  for (const auto& f : fragments) {
    if (!std::isnan(f.lineHeight) && f.lineHeight > 0) {
      fragmentHasExplicitLineHeight = true;
      break;
    }
  }
  // CSS `transform` is reproduced via `Layer.matrix` on the outer Layer (handled below by
  // applyBoxTransform); it does NOT force a TextBox to be synthesised. The earlier
  // codepath set TextBox.skew/rotation/scale and required an explicit TextBox to host
  // them, but the unified Layer.matrix path makes the wrapping Layer transform every
  // descendant uniformly — including the bare `<Text>+<Fill>` pair the no-TextBox branch
  // emits — so the extra TextBox is no longer needed.
  bool needsTextBox = hasMultipleFragments || !inherited.textAlign.empty() ||
                      !inherited.lineHeight.empty() || box.clipOverflow || hasNoWrap ||
                      isVertical || fragmentHasExplicitLineHeight;

  auto outer = _document->makeNode<Layer>();
  _layerBuilder.applySizeAndPosition(outer, box);
  _layerBuilder.applyLayerAttributes(outer, element, box);
  _layerBuilder.applyBoxTransform(outer, box, element);

  Layer* textHost = outer;
  if (hasBgVisuals) {
    _layerBuilder.applyBackgroundVisuals(outer, box);
    auto inner = _document->makeNode<Layer>();
    inner->percentWidth = 100.0f;
    inner->percentHeight = 100.0f;
    if (box.paddingSet) inner->padding = box.padding;
    outer->children.push_back(inner);
    textHost = inner;
  } else if (box.paddingSet) {
    outer->padding = box.padding;
  }

  // Hoist any non-inset `box-shadow` off `outer` when it also clips. `wrapper` becomes the
  // root of this subtree returned to the caller; `outer` (and `textHost`) keep their
  // existing role as the clipping/text host inside the wrapper.
  Layer* wrapper = _layerBuilder.maybeSplitBoxShadowFromClip(outer);

  if (!needsTextBox) {
    const auto& f = fragments.front();
    textHost->contents.push_back(buildTextElement(f));
    textHost->contents.push_back(buildTextFill(f));
  } else {
    auto textBox = _document->makeNode<TextBox>();
    // Anchor the TextBox to the host layer's content width so that wordWrap can engage. Without
    // this the TextBox would adopt its single-line natural width and overflow the parent box.
    textBox->percentWidth = 100.0f;
    std::string ta = ToLower(Trim(inherited.textAlign));
    if (!ta.empty()) {
      if (ta == "left" || ta == "start") textBox->textAlign = TextAlign::Start;
      else if (ta == "center")
        textBox->textAlign = TextAlign::Center;
      else if (ta == "right" || ta == "end")
        textBox->textAlign = TextAlign::End;
      else if (ta == "justify")
        textBox->textAlign = TextAlign::Justify;
      else
        _diagnostics.warn("html: unsupported text-align '" + ta + "'");
    }
    if (!inherited.lineHeight.empty()) {
      float lh = _valueParser.resolveLineHeightPx(inherited.lineHeight, fragments.front().fontSize);
      if (!std::isnan(lh) && lh > 0) textBox->lineHeight = lh;
    }
    // CSS line-box height = max of the parent's line-height and every inline child's
    // line-height. The block above only consulted the outer element's cascade, so it
    // would silently drop a child run's `line-height` whenever the outer was `normal` or
    // had no explicit value (for example the `<div height:20><span line-height:20>02</span></div>`
    // digit-badge pattern, where the outer span only carries background/size and the
    // inner span supplies the line-height that vertically centres the glyph). Walk the
    // fragments and lift their resolved values to TextBox.lineHeight when they exceed the
    // outer-driven value, mirroring the browser's max-of-participants rule.
    for (const auto& f : fragments) {
      if (!std::isnan(f.lineHeight) && f.lineHeight > textBox->lineHeight) {
        textBox->lineHeight = f.lineHeight;
      }
    }
    if (hasNoWrap) {
      textBox->wordWrap = false;
    }
    if (box.clipOverflow) {
      textBox->overflow = Overflow::Hidden;
    }
    if (isVertical) {
      textBox->writingMode = WritingMode::Vertical;
    }
    for (size_t i = 0; i < fragments.size(); i++) {
      const auto& f = fragments[i];
      if (i == 0) {
        textBox->elements.push_back(buildTextElement(f));
        textBox->elements.push_back(buildTextFill(f));
      } else {
        auto group = _document->makeNode<Group>();
        group->elements.push_back(buildTextElement(f));
        group->elements.push_back(buildTextFill(f));
        textBox->elements.push_back(group);
      }
    }
    textHost->contents.push_back(textBox);
  }

  std::string deco = ToLower(Trim(inherited.textDecoration));
  if (!deco.empty() && deco != "none") {
    Color textColor = fragments.front().color;
    Color decorationColor = textColor;
    bool decorationColorDiffers = false;
    if (!inherited.textDecorationColor.empty()) {
      Color parsed = _valueParser.parseColor(inherited.textDecorationColor);
      if (!(parsed == textColor)) {
        decorationColor = parsed;
        decorationColorDiffers = true;
      }
    }
    if (deco.find("underline") != std::string::npos) {
      _layerBuilder.emitTextDecorationLine(textHost, textColor, decorationColor,
                                           decorationColorDiffers,
                                           /*bottom=*/0.0f, /*centerY=*/NAN);
    }
    if (deco.find("line-through") != std::string::npos) {
      _layerBuilder.emitTextDecorationLine(textHost, textColor, decorationColor,
                                           decorationColorDiffers,
                                           /*bottom=*/NAN, /*centerY=*/0.0f);
    }
    if (deco.find("overline") != std::string::npos) {
      _diagnostics.warn("html: text-decoration overline not supported");
    }
  }

  _idAllocator.assign(wrapper, element);
  return wrapper;
}

}  // namespace pagx
