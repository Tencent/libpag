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

#include <string>
#include <unordered_map>

namespace pagx {

class HTMLIdAllocator;
class Image;
class PAGXDocument;

/**
 * Owns the importer-side bookkeeping for image resources: source-path normalisation, dedup
 * across `<img>` references that point at the same file, and id allocation for the synthetic
 * `Image` nodes attached to the document.
 *
 * The resolver borrows the importer's `HTMLIdAllocator` (so generated image ids participate
 * in the same uniqueness pool as DOM-side ids) and the document handle (deferred-bound after
 * `PAGXDocument` is constructed in `HTMLParserContext::parseDOM`).
 */
class HTMLImageResources {
 public:
  explicit HTMLImageResources(HTMLIdAllocator& idAllocator);

  /** Wires the document handle. Must be called once after the document has been constructed
   *  and before any `registerResource` call. */
  void bindDocument(PAGXDocument* document);

  /** Sets the directory used as the anchor for relative `<img src="...">` paths. Pass an empty
   *  string when parsing from in-memory bytes (relative paths cannot be resolved in that case
   *  and pass through unchanged). */
  void setBasePath(std::string basePath);

  /** Resolves a raw `src` URL to a final image source path: absolute URLs (file paths, schemes,
   *  data URIs, Windows drive letters) pass through; relative paths are prefixed with the
   *  configured base path. */
  std::string resolveSource(const std::string& src) const;

  /** Registers an image resource for the given (already-resolved) source path. Repeated calls
   *  with the same path return the same `Image*` so multiple `<img>` elements share the
   *  underlying resource. Returns nullptr when `imageSource` is empty. */
  Image* registerResource(const std::string& imageSource);

  /** Creates a fresh, unresolved `Image` node (empty `filePath`) with a unique id. Used to
   *  preserve the image slot of an `<img>` whose `src` is missing/empty/invalid so the element
   *  is not silently dropped from the PAGX. Never deduplicated — each placeholder is distinct. */
  Image* createPlaceholder();

 private:
  HTMLIdAllocator& _idAllocator;
  PAGXDocument* _document = nullptr;
  std::string _basePath = {};
  std::unordered_map<std::string, Image*> _sourceToImage = {};
};

}  // namespace pagx
