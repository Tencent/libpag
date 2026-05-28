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
#include <vector>
#include "pagx/PAGXDocument.h"

namespace pagx {

/**
 * Collects diagnostics produced during HTML import.
 *
 * The importer surfaces user-visible warnings on `PAGXDocument::errors`, but the document is
 * only constructed mid-way through `HTMLParserContext::parseDOM` (after canvas-size resolution).
 * Diagnostics raised before that point are buffered in `_pending` and flushed onto the document
 * once `bindDocument()` is called; subsequent diagnostics write through directly.
 *
 * `strict` mode upgrades every warning into a hard error so the importer can fail fast.
 */
class HTMLDiagnosticSink {
 public:
  explicit HTMLDiagnosticSink(bool strict);

  /**
   * Wires the sink to the destination document. Any diagnostics buffered before this call are
   * appended to `document->errors` in the order they were recorded; subsequent calls bypass the
   * buffer and append directly. The pointer is borrowed (no ownership taken).
   */
  void bindDocument(PAGXDocument* document);

  /**
   * Records a warning. In strict mode this is upgraded into a hard error.
   */
  void warn(const std::string& message);

  /**
   * Records a hard error and flips `hadHardError()` to true.
   */
  void hardError(const std::string& message);

  /**
   * Flips `hadHardError()` to true without emitting a new message. Used when the fatal
   * condition has already been surfaced (e.g. as the trailing entry of a transformer report
   * that was just forwarded through `warn()`), and the caller only needs the state flag.
   */
  void flagHardError();

  /**
   * True once any hard error has been recorded. Callers (the parser context) check this between
   * traversal phases to short-circuit further work and surface a nullptr document.
   */
  bool hadHardError() const {
    return _hadHardError;
  }

 private:
  void push(std::string message);

  bool _strict = false;
  PAGXDocument* _document = nullptr;
  std::vector<std::string> _pending = {};
  bool _hadHardError = false;
};

}  // namespace pagx
