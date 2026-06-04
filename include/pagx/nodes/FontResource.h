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

#include <memory>
#include <utility>
#include "pagx/nodes/FontProvider.h"
#include "pagx/nodes/Resource.h"
#include "pagx/types/Data.h"

namespace pagx {

/**
 * FontResource stores raw font bytes or a custom FontProvider supplied by ResourceLoader. A loader
 * may fill it immediately or retain it and call setData(), setBytes(), or setProvider() later.
 */
class FontResource : public Resource {
 public:
  /**
   * Creates an empty font resource for delayed loading.
   */
  static std::shared_ptr<FontResource> MakeEmpty();

  /**
   * Creates a FontResource by copying raw font bytes.
   * @param bytes pointer to font bytes.
   * @param length byte length.
   * @param ttcIndex TrueType collection index. Ignored for non-TTC font data.
   */
  static std::shared_ptr<FontResource> FromBytes(const void* bytes, size_t length,
                                                 int ttcIndex = 0);

  /**
   * Creates a FontResource from an existing Data object.
   * @param data raw font bytes.
   * @param ttcIndex TrueType collection index. Ignored for non-TTC font data.
   */
  static std::shared_ptr<FontResource> FromData(std::shared_ptr<Data> data, int ttcIndex = 0);

  /**
   * Creates a FontResource from a custom provider. Returns nullptr if provider is null.
   * @param provider custom font provider.
   */
  static std::shared_ptr<FontResource> MakeCustom(std::shared_ptr<FontProvider> provider);

  ResourceType resourceType() const override {
    return ResourceType::Font;
  }

  /**
   * Returns raw font bytes, or nullptr for provider-backed custom fonts.
   */
  std::shared_ptr<Data> data() const {
    return fontData;
  }

  /**
   * Returns the TrueType collection index associated with data().
   */
  int ttcIndex() const {
    return fontTtcIndex;
  }

  /**
   * Returns the custom provider, or nullptr for byte-backed fonts.
   */
  std::shared_ptr<FontProvider> provider() const {
    return fontProvider;
  }

  /**
   * Sets raw font bytes and notifies all listeners.
   * @param data raw font bytes.
   * @param ttcIndex TrueType collection index. Ignored for non-TTC font data.
   * @return true if data is non-null and non-empty.
   */
  bool setData(std::shared_ptr<Data> data, int ttcIndex = 0);

  /**
   * Copies raw font bytes into this resource and notifies all listeners.
   * @param bytes pointer to font bytes.
   * @param length byte length.
   * @param ttcIndex TrueType collection index. Ignored for non-TTC font data.
   * @return true if bytes were copied.
   */
  bool setBytes(const void* bytes, size_t length, int ttcIndex = 0);

  /**
   * Sets a custom provider and notifies all listeners.
   * @param provider custom font provider.
   * @return true if provider is non-null.
   */
  bool setProvider(std::shared_ptr<FontProvider> provider);

 private:
  FontResource() = default;
  FontResource(std::shared_ptr<Data> data, int ttcIndex)
      : fontData(std::move(data)), fontTtcIndex(ttcIndex) {
  }
  explicit FontResource(std::shared_ptr<FontProvider> provider)
      : fontProvider(std::move(provider)) {
  }

  std::shared_ptr<Data> fontData = nullptr;
  int fontTtcIndex = 0;
  std::shared_ptr<FontProvider> fontProvider = nullptr;
};

}  // namespace pagx
