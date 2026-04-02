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

#include "SystemFonts.h"

#if defined(__APPLE__)

#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <set>

namespace pagx {

static std::string StringFromCFString(CFStringRef cfString) {
  if (cfString == nullptr) {
    return {};
  }
  CFIndex length = CFStringGetLength(cfString);
  CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
  std::string result(static_cast<size_t>(maxSize), '\0');
  if (!CFStringGetCString(cfString, result.data(), maxSize, kCFStringEncodingUTF8)) {
    return {};
  }
  result.resize(strlen(result.c_str()));
  return result;
}

static std::string StringFromCFURL(CFURLRef cfURL) {
  if (cfURL == nullptr) {
    return {};
  }
  CFStringRef cfPath = CFURLCopyFileSystemPath(cfURL, kCFURLPOSIXPathStyle);
  if (cfPath == nullptr) {
    return {};
  }
  auto path = StringFromCFString(cfPath);
  CFRelease(cfPath);
  return path;
}

static FontLocation GetFontLocationFromDescriptor(CTFontDescriptorRef descriptor) {
  // Get the PostScript name and family name from the descriptor to identify the exact face.
  CTFontRef ctFont = CTFontCreateWithFontDescriptor(descriptor, 0.0, nullptr);
  if (ctFont == nullptr) {
    return {};
  }
  CFStringRef psName = CTFontCopyPostScriptName(ctFont);
  auto cfURL = static_cast<CFURLRef>(CTFontCopyAttribute(ctFont, kCTFontURLAttribute));
  CFRelease(ctFont);

  if (cfURL == nullptr) {
    cfURL = static_cast<CFURLRef>(CTFontDescriptorCopyAttribute(descriptor, kCTFontURLAttribute));
  }
  if (cfURL == nullptr) {
    if (psName != nullptr) {
      CFRelease(psName);
    }
    return {};
  }

  FontLocation location = {};
  location.path = StringFromCFURL(cfURL);

  // Read fontFamily from the descriptor (no Typeface creation needed).
  auto cfFamily = static_cast<CFStringRef>(
      CTFontDescriptorCopyAttribute(descriptor, kCTFontFamilyNameAttribute));
  if (cfFamily != nullptr) {
    location.fontFamily = StringFromCFString(cfFamily);
    CFRelease(cfFamily);
  }
  auto cfStyle = static_cast<CFStringRef>(
      CTFontDescriptorCopyAttribute(descriptor, kCTFontStyleNameAttribute));
  if (cfStyle != nullptr) {
    location.fontStyle = StringFromCFString(cfStyle);
    CFRelease(cfStyle);
  }

  // Determine the face index within a TTC file by enumerating all descriptors for the same URL
  // and matching by PostScript name.
  if (psName != nullptr) {
    CFArrayRef allDescriptors = CTFontManagerCreateFontDescriptorsFromURL(cfURL);
    if (allDescriptors != nullptr) {
      CFIndex total = CFArrayGetCount(allDescriptors);
      for (CFIndex j = 0; j < total; j++) {
        auto desc = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(allDescriptors, j));
        auto name =
            static_cast<CFStringRef>(CTFontDescriptorCopyAttribute(desc, kCTFontNameAttribute));
        if (name != nullptr) {
          if (CFStringCompare(name, psName, 0) == kCFCompareEqualTo) {
            location.ttcIndex = static_cast<int>(j);
            CFRelease(name);
            break;
          }
          CFRelease(name);
        }
      }
      CFRelease(allDescriptors);
    }
    CFRelease(psName);
  }
  CFRelease(cfURL);
  return location;
}

std::vector<FontLocation> SystemFonts::FallbackTypefaces() {
  CTFontRef defaultFont = CTFontCreateWithName(CFSTR("Helvetica"), 12.0, nullptr);
  if (defaultFont == nullptr) {
    defaultFont = CTFontCreateUIFontForLanguage(kCTFontUIFontSystem, 12.0, nullptr);
  }
  if (defaultFont == nullptr) {
    return {};
  }

  CFArrayRef languages = CFLocaleCopyPreferredLanguages();
  CFArrayRef cascadeList = CTFontCopyDefaultCascadeListForLanguages(defaultFont, languages);
  CFRelease(defaultFont);
  if (languages != nullptr) {
    CFRelease(languages);
  }
  if (cascadeList == nullptr) {
    return {};
  }

  CFIndex count = CFArrayGetCount(cascadeList);
  std::vector<FontLocation> fallbacks = {};
  fallbacks.reserve(static_cast<size_t>(count));
  std::set<std::string> loadedPaths = {};

  // The cascade list is ordered by the user's language preferences. For fonts sharing the same
  // file (e.g., PingFang SC/TC/HK in PingFang.ttc), only the first one matching the preferred
  // language is kept.
  for (CFIndex i = 0; i < count; i++) {
    auto descriptor = static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(cascadeList, i));
    if (descriptor == nullptr) {
      continue;
    }
    auto location = GetFontLocationFromDescriptor(descriptor);
    if (location.path.empty() || loadedPaths.count(location.path) > 0) {
      continue;
    }
    loadedPaths.insert(location.path);
    fallbacks.push_back(std::move(location));
  }
  CFRelease(cascadeList);
  return fallbacks;
}

}  // namespace pagx

#elif defined(_WIN32)

#include <dwrite.h>
#include <string>

#pragma comment(lib, "dwrite.lib")

namespace pagx {

template <class T>
static void SafeRelease(T** ppT) {
  if (*ppT) {
    (*ppT)->Release();
    *ppT = nullptr;
  }
}

static std::string WideToUTF8(const wchar_t* wide, int length) {
  if (length == 0) {
    return {};
  }
  int size = WideCharToMultiByte(CP_UTF8, 0, wide, length, nullptr, 0, nullptr, nullptr);
  std::string result(static_cast<size_t>(size), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide, length, result.data(), size, nullptr, nullptr);
  return result;
}

static std::string GetFamilyName(IDWriteFontFamily* fontFamily) {
  IDWriteLocalizedStrings* names = nullptr;
  HRESULT hr = fontFamily->GetFamilyNames(&names);
  if (FAILED(hr) || names == nullptr) {
    return {};
  }

  UINT32 index = 0;
  BOOL exists = FALSE;
  names->FindLocaleName(L"en-us", &index, &exists);
  if (!exists) {
    index = 0;
  }

  UINT32 length = 0;
  hr = names->GetStringLength(index, &length);
  if (FAILED(hr) || length == 0) {
    SafeRelease(&names);
    return {};
  }

  std::wstring wide(static_cast<size_t>(length) + 1, L'\0');
  hr = names->GetString(index, wide.data(), length + 1);
  SafeRelease(&names);
  if (FAILED(hr)) {
    return {};
  }

  return WideToUTF8(wide.c_str(), static_cast<int>(length));
}

std::vector<FontLocation> SystemFonts::FallbackTypefaces() {
  IDWriteFactory* factory = nullptr;
  HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&factory));
  if (FAILED(hr) || factory == nullptr) {
    return {};
  }

  IDWriteFontCollection* fontCollection = nullptr;
  hr = factory->GetSystemFontCollection(&fontCollection);
  if (FAILED(hr) || fontCollection == nullptr) {
    SafeRelease(&factory);
    return {};
  }

  UINT32 familyCount = fontCollection->GetFontFamilyCount();
  std::vector<FontLocation> fallbacks = {};
  fallbacks.reserve(familyCount);

  for (UINT32 i = 0; i < familyCount; i++) {
    IDWriteFontFamily* fontFamily = nullptr;
    hr = fontCollection->GetFontFamily(i, &fontFamily);
    if (FAILED(hr) || fontFamily == nullptr) {
      continue;
    }

    auto familyName = GetFamilyName(fontFamily);
    SafeRelease(&fontFamily);
    if (familyName.empty()) {
      continue;
    }

    FontLocation location = {};
    location.fontFamily = std::move(familyName);
    fallbacks.push_back(std::move(location));
  }

  SafeRelease(&fontCollection);
  SafeRelease(&factory);
  return fallbacks;
}

}  // namespace pagx

#elif defined(__linux__)

#include <fontconfig/fontconfig.h>
#include <unistd.h>
#include <set>
#include <string>

namespace pagx {

std::vector<FontLocation> SystemFonts::FallbackTypefaces() {
  FcPattern* pattern = FcPatternCreate();
  if (pattern == nullptr) {
    return {};
  }

  FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
  FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  FcResult result = FcResultMatch;
  FcFontSet* fontSet = FcFontSort(nullptr, pattern, FcTrue, nullptr, &result);
  FcPatternDestroy(pattern);
  if (fontSet == nullptr) {
    return {};
  }

  std::vector<FontLocation> fallbacks = {};
  fallbacks.reserve(static_cast<size_t>(fontSet->nfont));
  std::set<std::string> loadedFamilies = {};

  for (int i = 0; i < fontSet->nfont; i++) {
    FcPattern* font = fontSet->fonts[i];

    FcBool isScalable = FcFalse;
    if (FcPatternGetBool(font, FC_SCALABLE, 0, &isScalable) != FcResultMatch || !isScalable) {
      continue;
    }

    FcChar8* filePath = nullptr;
    if (FcPatternGetString(font, FC_FILE, 0, &filePath) != FcResultMatch) {
      continue;
    }

    if (access(reinterpret_cast<const char*>(filePath), R_OK) != 0) {
      continue;
    }

    FcChar8* familyName = nullptr;
    if (FcPatternGetString(font, FC_FAMILY, 0, &familyName) != FcResultMatch) {
      continue;
    }
    auto family = std::string(reinterpret_cast<const char*>(familyName));

    if (loadedFamilies.count(family) > 0) {
      continue;
    }
    loadedFamilies.insert(family);

    int ttcIndex = 0;
    FcPatternGetInteger(font, FC_INDEX, 0, &ttcIndex);

    FontLocation location = {};
    location.path = std::string(reinterpret_cast<const char*>(filePath));
    location.fontFamily = family;
    location.ttcIndex = ttcIndex;
    fallbacks.push_back(std::move(location));
  }

  FcFontSetDestroy(fontSet);
  return fallbacks;
}

}  // namespace pagx

#else

namespace pagx {

std::vector<FontLocation> SystemFonts::FallbackTypefaces() {
  return {};
}

}  // namespace pagx

#endif
