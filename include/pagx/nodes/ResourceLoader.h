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

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace pagx {

class Composition;
class Font;
class Image;

/**
 * ResourceType identifies the kind of external resource requested during PAGXDocument import.
 */
enum class ResourceType : uint8_t {
  Image = 0,
  Font = 1,
  Composition = 2,
};

/**
 * Describes one resource load request made during PAGXDocument import. The loader receives the
 * resource type, XML id, original source string, and custom data (`data-*` attributes without the
 * prefix) so business code can decide whether to handle the resource.
 */
struct ResourceLoadRequest {
  /**
   * The kind of resource being requested.
   */
  ResourceType type = ResourceType::Image;

  /**
   * The XML id of the node requesting the resource. May be empty if the source node has no id.
   */
  std::string id = {};

  /**
   * The original source string from the XML, such as an image path, font source, or PAGX path.
   */
  std::string source = {};

  /**
   * Custom `data-*` attributes from the source node, stored without the `data-` prefix.
   */
  std::unordered_map<std::string, std::string> customData = {};
};

/**
 * Per-document-load resource loader. It mirrors Rive's FileAssetLoader model: the importer passes
 * the mutable asset node to the loader, and the loader returns whether it takes responsibility for
 * that asset. Return true after handling or taking over the asset; the importer will not run its
 * built-in fallback. Return false to let the importer continue with its normal fallback path.
 *
 * The call itself is synchronous, but returning true does not require the asset to be fully loaded
 * immediately. A loader may leave the node partially filled for a higher-level owner to complete
 * later, as long as it intentionally takes over that responsibility.
 */
class ResourceLoader {
 public:
  virtual ~ResourceLoader() = default;

  /**
   * Attempts to handle an image resource. When returning true, fill `image` as needed, for example
   * by setting Image::data to encoded image bytes, or intentionally take over later loading. Return
   * false to let the importer decode data URIs or keep Image::filePath for loadFileData().
   * @param request metadata describing the image source.
   * @param image the mutable Image node created by the importer.
   */
  virtual bool loadImage(const ResourceLoadRequest& request, Image* image) {
    (void)request;
    (void)image;
    return false;
  }

  /**
   * Attempts to handle a font resource. When returning true, fill `font` as needed or intentionally
   * take over later loading; embedded Glyph fallback will be skipped. Return false to let the
   * importer parse embedded Glyph children.
   * @param request metadata describing the font source.
   * @param font the mutable Font node created by the importer.
   */
  virtual bool loadFont(const ResourceLoadRequest& request, Font* font) {
    (void)request;
    (void)font;
    return false;
  }

  /**
   * Attempts to handle a composition resource. When returning true, fill `composition` with the
   * loaded document contents, typically by assigning width, height, layers, animations, and
   * externalDoc. Return false to let the importer try file-path loading or keep compositionFilePath
   * for loadFileData().
   * @param request metadata describing the composition source.
   * @param composition the mutable Composition node created by the importer.
   */
  virtual bool loadComposition(const ResourceLoadRequest& request, Composition* composition) {
    (void)request;
    (void)composition;
    return false;
  }
};

}  // namespace pagx
