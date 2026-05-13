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

#include "cli/CommandExport.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "cli/CliUtils.h"
#include "pagx/FontConfig.h"
#include "pagx/HTMLExporter.h"
#include "pagx/PAGXImporter.h"
#include "pagx/PPTExporter.h"
#include "pagx/SVGExporter.h"
#include "tgfx/core/Typeface.h"

namespace pagx::cli {

struct ExportOptions {
  std::string inputFile = {};
  std::string outputFile = {};
  std::string format = {};
  int svgIndent = 2;
  bool svgNoXmlDeclaration = false;
  bool textToPath = false;
  bool pptBakeUnsupported = true;
  std::vector<std::string> htmlFonts = {};
};

static void PrintUsage() {
  std::cout
      << "Usage: pagx export [options]\n"
      << "\n"
      << "Export a PAGX file to another format.\n"
      << "\n"
      << "Options:\n"
      << "  --input <file>              Input PAGX file (required)\n"
      << "  --output <file>             Output file (default: <input>.<format>)\n"
      << "  --format <format>           Output format (svg, pptx, html; inferred from --output "
         "extension)\n"
      << "  --text-to-path              Convert text to path geometry (default: native text)\n"
      << "\n"
      << "SVG options:\n"
      << "  --svg-indent <n>            Indentation spaces (default: 2, valid range: 0-16)\n"
      << "  --svg-no-xml-declaration    Omit the <?xml ...?> declaration\n"
      << "\n"
      << "PPT options:\n"
      << "  --ppt-no-bake-unsupported\n"
      << "                              Do not bake layers that use features OOXML cannot\n"
      << "                              represent natively (masks, scrollRect clipping, blend\n"
      << "                              modes outside of Normal/Multiply/Screen/Darken/Lighten,\n"
      << "                              wide-gamut color, and BackgroundBlurStyle) into PNG\n"
      << "                              patches. By default these layers are baked so the slide\n"
      << "                              matches the tgfx renderer; for unsupported blend modes\n"
      << "                              and BackgroundBlurStyle the backdrop beneath the layer\n"
      << "                              is baked too so the composite (or frosted-glass blur)\n"
      << "                              renders correctly, at the cost of turning native content\n"
      << "                              under the patch into pixels. Pass this flag to silently\n"
      << "                              drop those features and emit the layer as editable\n"
      << "                              shapes instead (mask ignored, scrollRect dropped, blend\n"
      << "                              falls back to Normal, wide-gamut clamped to sRGB).\n"
      << "\n"
      << "HTML options:\n"
      << "  --html-font <spec>          Inject a CSS @font-face rule. Can be repeated.\n"
      << "                              Spec syntax:\n"
      << "                                "
         "<abs-path|https-url>[#family=...][#weight=...][#style=...][#unicode-range=...]\n"
      << "                              Local files must be absolute paths; file is copied to\n"
      << "                              <output-dir>/fonts/ and referenced as "
         "url(\"fonts/<name>\").\n"
      << "                              Family/weight/style are auto-detected from the file;\n"
      << "                              override any with #family=/#weight=/#style=.\n"
      << "                              URLs (http/https) require all three: #family, #weight, "
         "#style.\n"
      << "                              #unicode-range is optional; use it to scope a rule\n"
      << "                              to a codepoint subset (e.g. emoji as\n"
      << "                              U+1F300-1F9FF) so multiple @font-face rules under\n"
      << "                              the same family cover different scripts.\n"
      << "\n"
      << "Examples:\n"
      << "  pagx export --input icon.pagx                    # PAGX to icon.svg\n"
      << "  pagx export --input icon.pagx --output out.svg   # PAGX to out.svg\n"
      << "  pagx export --input icon.pagx --output out.pptx  # PAGX to out.pptx\n"
      << "  pagx export --format svg --input icon.pagx       # force SVG output format\n"
      << "  pagx export --format pptx --input icon.pagx      # force PPTX output format\n"
      << "  pagx export --input icon.pagx --svg-indent 4     # 4-space indent\n"
      << "  pagx export --input icon.pagx --text-to-path     # convert text to paths\n"
      << "  pagx export --input icon.pagx --output out.pptx --ppt-no-bake-unsupported\n"
      << "                                                   # keep unsupported features "
         "editable\n"
      << "  pagx export --input icon.pagx --output icon.html # PAGX to HTML\n"
      << "  pagx export --input dash.pagx --output out/index.html \\\n"
      << "              --html-font /abs/NotoSansSC-Regular.otf \\\n"
      << "              --html-font /abs/NotoSansSC-Bold.ttf  # auto-detect family/weight\n"
      << "  pagx export --input dash.pagx --output out/index.html \\\n"
      << "              --html-font "
         "'https://cdn.example.com/roboto.woff2#family=Roboto#weight=400#style=normal'\n";
}

static int ParseOptions(int argc, char* argv[], ExportOptions* options) {
  int i = 1;
  while (i < argc) {
    std::string arg = argv[i];
    if (arg == "--input" && i + 1 < argc) {
      options->inputFile = argv[++i];
    } else if (arg == "--output" && i + 1 < argc) {
      options->outputFile = argv[++i];
    } else if (arg == "--format" && i + 1 < argc) {
      options->format = argv[++i];
    } else if (arg == "--svg-indent" && i + 1 < argc) {
      char* endPtr = nullptr;
      long value = strtol(argv[++i], &endPtr, 10);
      if (endPtr == argv[i] || *endPtr != '\0' || value < 0 || value > 16) {
        std::cerr << "pagx export: error: invalid indent '" << argv[i] << "' (must be 0-16)\n";
        return 1;
      }
      options->svgIndent = static_cast<int>(value);
    } else if (arg == "--svg-no-xml-declaration") {
      options->svgNoXmlDeclaration = true;
    } else if (arg == "--text-to-path") {
      options->textToPath = true;
    } else if (arg == "--ppt-no-bake-unsupported") {
      options->pptBakeUnsupported = false;
    } else if (arg == "--html-font" && i + 1 < argc) {
      options->htmlFonts.emplace_back(argv[++i]);
    } else if (arg == "--help" || arg == "-h") {
      PrintUsage();
      return -1;
    } else if (arg[0] == '-') {
      std::cerr << "pagx export: error: unknown option '" << arg << "'\n";
      return 1;
    } else {
      std::cerr << "pagx export: error: unexpected argument '" << arg
                << "', use --input to specify the input file\n";
      return 1;
    }
    i++;
  }

  if (options->inputFile.empty()) {
    std::cerr << "pagx export: error: missing --input file\n";
    return 1;
  }

  if (options->format.empty() && !options->outputFile.empty()) {
    options->format = GetFileExtension(options->outputFile);
  }
  if (options->format.empty()) {
    std::cerr << "pagx export: error: cannot infer output format, use --format or specify an "
                 "output file with a known extension\n";
    return 1;
  }

  if (options->outputFile.empty()) {
    options->outputFile = ReplaceExtension(options->inputFile, options->format);
  }

  return 0;
}

static int ExportToSVG(const ExportOptions& options) {
  auto document = LoadDocument(options.inputFile, "pagx export");
  if (document == nullptr) {
    return 1;
  }
  if (document->hasUnresolvedImports()) {
    std::cerr << "pagx export: error: unresolved import directive, run 'pagx resolve' first\n";
    return 1;
  }

  SVGExporter::Options svgOptions = {};
  svgOptions.indent = options.svgIndent;
  svgOptions.xmlDeclaration = !options.svgNoXmlDeclaration;
  svgOptions.convertTextToPath = options.textToPath;

  if (!SVGExporter::ToFile(*document, options.outputFile, svgOptions)) {
    std::cerr << "pagx export: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx export: wrote " << options.outputFile << "\n";
  return 0;
}

// Parsed representation of a single --html-font spec — one spec maps to one source entry.
// Multiple ParsedFontFace instances sharing the same (fontFamily, fontWeight, fontStyle) triple
// are later aggregated into a single FontFaceRule with multiple sources, so that users can
// express "prefer local, fall back to CDN" by repeating --html-font with the same triple.
// For local-mode entries, `sourcePath` holds the absolute filesystem path of the font file to
// be copied into <output-dir>/fonts/; the final src URL is filled in by the copy step.
// `typeface` holds a loaded tgfx::Typeface when the local file could be parsed, so that the
// caller can register it with pagx::FontConfig and apply accurate text metrics during layout
// — without this, applyLayout would fall back to system fonts and produce line-heights that
// drift from what the browser actually renders with the injected @font-face.
// For URL-mode entries, `sourcePath` and `typeface` are empty and `source.uri` is the user URL.
struct ParsedFontFace {
  std::string fontFamily = {};
  std::string fontWeight = {};
  std::string fontStyle = {};
  std::string unicodeRange = {};  // empty → applies to every codepoint in the source font
  FontFaceSource source = {};
  std::string sourcePath = {};                    // empty for URL mode
  std::shared_ptr<tgfx::Typeface> typeface = {};  // non-null for local mode when parse succeeded
};

// Splits a single --html-font spec into uri + #key=value overrides. The separator is '#' to
// avoid conflict with Windows drive letters (':') and font-family names (',' / '='). Fragments
// are consumed greedily from the right so that the recogniser stops at the first token that
// doesn't look like a valid "key=value" — anything to its left (including real URL fragments
// like https://x/y#frag) stays part of the URI. Recognised keys are family/weight/style and
// unicode-range (the last scopes a rule to a subset of codepoints so multiple @font-face
// rules under the same family can cover different scripts, e.g. Noto Sans SC for Latin/CJK
// alongside Noto Color Emoji limited to emoji codepoints).
static bool ParseFontFaceOverrides(const std::string& spec, std::string* uri, std::string* family,
                                   std::string* weight, std::string* style,
                                   std::string* unicodeRange) {
  std::string rest = spec;
  while (true) {
    auto hashPos = rest.rfind('#');
    if (hashPos == std::string::npos) {
      break;
    }
    auto eqPos = rest.find('=', hashPos + 1);
    if (eqPos == std::string::npos) {
      break;  // trailing "#foo" with no '=' — leave it attached to the URI
    }
    std::string key = rest.substr(hashPos + 1, eqPos - hashPos - 1);
    std::string value = rest.substr(eqPos + 1);
    std::string* target = nullptr;
    if (key == "family") {
      target = family;
    } else if (key == "weight") {
      target = weight;
    } else if (key == "style") {
      target = style;
    } else if (key == "unicode-range") {
      target = unicodeRange;
    }
    if (target == nullptr) {
      break;  // unknown key — stop right-to-left consumption, rest belongs to the URI
    }
    if (!target->empty()) {
      std::cerr << "pagx export: error: duplicate #" << key << " in --html-font spec '" << spec
                << "'\n";
      return false;
    }
    *target = value;
    rest.erase(hashPos);
  }
  *uri = rest;
  return true;
}

// Parses one --html-font spec into a ParsedFontFace. Returns true on success. On failure, prints
// a specific diagnostic to stderr and returns false. See the PrintUsage text for the spec syntax.
static bool ParseFontFaceSpec(const std::string& spec, ParsedFontFace* out) {
  std::string uri, familyOverride, weightOverride, styleOverride, unicodeRangeOverride;
  if (!ParseFontFaceOverrides(spec, &uri, &familyOverride, &weightOverride, &styleOverride,
                              &unicodeRangeOverride)) {
    return false;
  }
  if (uri.empty()) {
    std::cerr << "pagx export: error: empty URI in --html-font spec '" << spec << "'\n";
    return false;
  }
  const bool isUrl = uri.rfind("http://", 0) == 0 || uri.rfind("https://", 0) == 0;
  if (isUrl) {
    if (familyOverride.empty() || weightOverride.empty() || styleOverride.empty()) {
      std::cerr << "pagx export: error: --html-font URL-mode spec '" << spec
                << "' requires #family=..., #weight=..., and #style=...\n";
      return false;
    }
    out->fontFamily = familyOverride;
    out->fontWeight = weightOverride;
    out->fontStyle = styleOverride;
    out->unicodeRange = unicodeRangeOverride;
    out->source.uri = uri;
    out->source.mode = FontEmbedMode::URL;
    out->sourcePath.clear();
    return true;
  }
  // Local file mode
  std::filesystem::path path(uri);
  if (!path.is_absolute()) {
    std::cerr << "pagx export: error: --html-font local path must be absolute; got '" << uri
              << "'\n";
    return false;
  }
  std::error_code ec;
  if (!std::filesystem::exists(path, ec) || ec) {
    std::cerr << "pagx export: error: --html-font file does not exist: '" << uri << "'\n";
    return false;
  }
  // Always attempt to load the typeface, even when the user provided overrides for every
  // field. The loaded object is handed back so that applyLayout can register it and compute
  // text metrics with the real font rather than a system fallback; otherwise the hard-coded
  // widths/line-heights the exporter writes into CSS would disagree with what the browser
  // renders with the same font file.
  auto typeface = tgfx::Typeface::MakeFromPath(uri);
  std::string autoFamily, autoWeight, autoStyle;
  const bool needAutoFields =
      familyOverride.empty() || weightOverride.empty() || styleOverride.empty();
  if (needAutoFields) {
    if (typeface == nullptr) {
      std::cerr << "pagx export: error: --html-font failed to parse font file '" << uri
                << "'; override with #family=/#weight=/#style=\n";
      return false;
    }
    autoFamily = typeface->fontFamily();
    auto styleName = typeface->fontStyle();
    if (!MapFontStyleToCSS(styleName, &autoWeight, &autoStyle)) {
      if (weightOverride.empty() || styleOverride.empty()) {
        std::cerr << "pagx export: error: --html-font cannot map font style '" << styleName
                  << "' for '" << uri << "'; override with #weight=/#style=\n";
        return false;
      }
    }
    if (autoFamily.empty() && familyOverride.empty()) {
      std::cerr << "pagx export: error: --html-font cannot read font family from '" << uri
                << "'; override with #family=\n";
      return false;
    }
  }
  out->fontFamily = familyOverride.empty() ? autoFamily : familyOverride;
  out->fontWeight = weightOverride.empty() ? autoWeight : weightOverride;
  out->fontStyle = styleOverride.empty() ? autoStyle : styleOverride;
  out->unicodeRange = unicodeRangeOverride;
  out->source.uri = {};                   // filled in by copy step
  out->source.mode = FontEmbedMode::URL;  // referenced by relative URL after copy
  out->sourcePath = std::filesystem::absolute(path).string();
  out->typeface = std::move(typeface);
  return true;
}

// Wraps the HTML fragment returned by HTMLExporter::ToHTML in a complete <!DOCTYPE html>
// document so that the output file can be opened directly in a browser. The body is sized to
// the PAGX canvas; font-synthesis is disabled at the body level so that browsers don't spray
// auto-synthesised bold/italic onto elements that did not ask for it — the per-element
// font-synthesis:auto toggles emitted by HTMLExporter re-enable it where needed.
static std::string WrapAsHTMLDocument(const std::string& fragment, float width, float height) {
  auto w = std::to_string(static_cast<int>(width));
  auto h = std::to_string(static_cast<int>(height));
  std::string bodyStyle = "margin:0;padding:0;width:" + w + "px;height:" + h +
                          "px;overflow:hidden;font-family:sans-serif;font-synthesis:none";
  return "<!DOCTYPE html>\n<html>\n<head>\n<meta charset=\"utf-8\">\n<style>\nbody { " + bodyStyle +
         " }\n</style>\n</head>\n<body>\n" + fragment + "\n</body>\n</html>\n";
}

static int ExportToHTML(const ExportOptions& options) {
  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx export: error: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  for (auto& error : document->errors) {
    std::cerr << "pagx export: warning: " << error << "\n";
  }

  // Parse every --html-font spec before touching the filesystem, so that syntax errors abort
  // cleanly without leaving half-populated output directories behind.
  std::vector<ParsedFontFace> fonts;
  fonts.reserve(options.htmlFonts.size());
  for (const auto& spec : options.htmlFonts) {
    ParsedFontFace parsed = {};
    if (!ParseFontFaceSpec(spec, &parsed)) {
      return 1;
    }
    fonts.push_back(std::move(parsed));
  }

  std::filesystem::path outputPath(options.outputFile);
  auto outputDir = outputPath.parent_path();
  if (outputDir.empty()) {
    outputDir = std::filesystem::current_path();
  }

  // Copy local-mode font files into <output-dir>/fonts/. Same-basename collisions are resolved
  // by overwriting with a warning so the command stays predictable; users who care about the
  // difference should give their font files unique names.
  auto fontsDir = outputDir / "fonts";
  bool fontsDirCreated = false;
  for (auto& parsed : fonts) {
    if (parsed.sourcePath.empty()) {
      continue;
    }
    if (!fontsDirCreated) {
      std::error_code ec;
      std::filesystem::create_directories(fontsDir, ec);
      if (ec) {
        std::cerr << "pagx export: error: failed to create directory '" << fontsDir.string()
                  << "': " << ec.message() << "\n";
        return 1;
      }
      fontsDirCreated = true;
    }
    auto basename = std::filesystem::path(parsed.sourcePath).filename().string();
    auto destPath = fontsDir / basename;
    std::error_code existsEc;
    if (std::filesystem::exists(destPath, existsEc) && !existsEc) {
      std::error_code sameEc;
      bool isSameFile = std::filesystem::equivalent(parsed.sourcePath, destPath, sameEc);
      if (sameEc || !isSameFile) {
        std::cerr << "pagx export: warning: overwriting '" << destPath.string() << "' with '"
                  << parsed.sourcePath << "'\n";
      }
    }
    std::error_code copyEc;
    std::filesystem::copy_file(parsed.sourcePath, destPath,
                               std::filesystem::copy_options::overwrite_existing, copyEc);
    if (copyEc) {
      std::cerr << "pagx export: error: failed to copy '" << parsed.sourcePath << "' to '"
                << destPath.string() << "': " << copyEc.message() << "\n";
      return 1;
    }
    parsed.source.uri = "fonts/" + basename;
  }

  HTMLExporter::Options htmlOptions = {};
  // Aggregate ParsedFontFace entries sharing the same (family, weight, style, unicode-range)
  // tuple into a single FontFaceRule with multiple sources. Browsers try sources left-to-right
  // and fall back to the next one on load failure, so users can list a local path followed by
  // a CDN URL to get "prefer local, CDN safety net" in one declaration. unicode-range is part
  // of the aggregation key because a rule scoping Noto Color Emoji to emoji codepoints is a
  // different @font-face from the unrestricted Noto Sans SC rule, even though both share the
  // same CSS font-family name. Preserve the user's CLI order both across rules (first unseen
  // tuple → first rule) and within a rule (sources appended in the order the user listed them).
  htmlOptions.fontFaceRules.reserve(fonts.size());
  for (const auto& parsed : fonts) {
    FontFaceRule* target = nullptr;
    for (auto& rule : htmlOptions.fontFaceRules) {
      if (rule.fontFamily == parsed.fontFamily && rule.fontWeight == parsed.fontWeight &&
          rule.fontStyle == parsed.fontStyle && rule.unicodeRange == parsed.unicodeRange) {
        target = &rule;
        break;
      }
    }
    if (target == nullptr) {
      FontFaceRule rule = {};
      rule.fontFamily = parsed.fontFamily;
      rule.fontWeight = parsed.fontWeight;
      rule.fontStyle = parsed.fontStyle;
      rule.unicodeRange = parsed.unicodeRange;
      htmlOptions.fontFaceRules.push_back(std::move(rule));
      target = &htmlOptions.fontFaceRules.back();
    }
    target->sources.push_back(parsed.source);
  }

  // Register local-mode font files with pagx::FontConfig so applyLayout computes text
  // metrics (glyph widths, line-heights) from the same font file the browser will load
  // via the generated @font-face rule. Without this step tgfx falls back to whatever
  // system font matches the requested family name, its metrics diverge from the browser's,
  // and the hard-coded CSS widths/line-heights the exporter emits no longer line up with
  // the actual rendered text. URL-mode fonts cannot be registered because the CLI has no
  // local bytes to read — their text will still layout against system fallbacks.
  pagx::FontConfig layoutFontConfig;
  for (auto& parsed : fonts) {
    if (parsed.typeface) {
      layoutFontConfig.registerTypeface(parsed.typeface);
    }
  }
  document->applyLayout(&layoutFontConfig);

  // Resource directory convention: sibling directory named after the HTML file's stem, so the
  // generated HTML references assets via "<stem>/…" relative URLs. This matches the new
  // HTMLExporter contract enforced by ToFile, but we call ToHTML explicitly because CLI needs
  // to wrap the fragment in a complete <!DOCTYPE html> document before writing it.
  auto outputStem = outputPath.stem().string();
  auto resourceDir = (outputDir / outputStem).string();
  auto fragment = HTMLExporter::ToHTML(*document, resourceDir, htmlOptions);
  if (fragment.empty()) {
    std::cerr << "pagx export: error: HTMLExporter produced empty output for '" << options.inputFile
              << "'\n";
    return 1;
  }

  auto html = WrapAsHTMLDocument(fragment, document->width, document->height);

  if (!outputDir.empty() && !std::filesystem::exists(outputDir)) {
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec) {
      std::cerr << "pagx export: error: failed to create directory '" << outputDir.string()
                << "': " << ec.message() << "\n";
      return 1;
    }
  }
  std::ofstream file(options.outputFile, std::ios::binary);
  if (!file) {
    std::cerr << "pagx export: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }
  file.write(html.data(), static_cast<std::streamsize>(html.size()));
  file.close();

  std::cout << "pagx export: wrote " << options.outputFile << "\n";
  return 0;
}

static int ExportToPPT(const ExportOptions& options) {
  auto document = PAGXImporter::FromFile(options.inputFile);
  if (document == nullptr) {
    std::cerr << "pagx export: error: failed to load '" << options.inputFile << "'\n";
    return 1;
  }
  if (!document->errors.empty()) {
    for (auto& error : document->errors) {
      std::cerr << "pagx export: warning: " << error << "\n";
    }
  }
  if (document->hasUnresolvedImports()) {
    std::cerr << "pagx export: error: unresolved import directive, run 'pagx resolve' first\n";
    return 1;
  }

  PPTExporter::Options pptOptions = {};
  pptOptions.convertTextToPath = options.textToPath;
  pptOptions.bakeUnsupported = options.pptBakeUnsupported;

  if (!PPTExporter::ToFile(*document, options.outputFile, pptOptions)) {
    std::cerr << "pagx export: error: failed to write '" << options.outputFile << "'\n";
    return 1;
  }

  std::cout << "pagx export: wrote " << options.outputFile << "\n";
  return 0;
}

int RunExport(int argc, char* argv[]) {
  ExportOptions options = {};
  auto parseResult = ParseOptions(argc, argv, &options);
  if (parseResult != 0) {
    return parseResult == -1 ? 0 : parseResult;
  }

  if (options.format == "svg") {
    return ExportToSVG(options);
  }
  if (options.format == "pptx") {
    return ExportToPPT(options);
  }

  if (options.format == "html") {
    return ExportToHTML(options);
  }

  std::cerr << "pagx export: error: unsupported format '" << options.format << "'\n";
  return 1;
}

}  // namespace pagx::cli
