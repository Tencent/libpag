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

#include "pagx/html/importer/HTMLDiagnosticSink.h"
#include <utility>

namespace pagx {

HTMLDiagnosticSink::HTMLDiagnosticSink(bool strict) : _strict(strict) {
}

void HTMLDiagnosticSink::bindDocument(PAGXDocument* document) {
  _document = document;
  if (_document == nullptr) {
    return;
  }
  for (auto& msg : _pending) {
    _document->errors.push_back(std::move(msg));
  }
  _pending.clear();
}

void HTMLDiagnosticSink::warn(const std::string& message) {
  if (_strict) {
    hardError(message);
    return;
  }
  push(message);
}

void HTMLDiagnosticSink::hardError(const std::string& message) {
  _hadHardError = true;
  push(message);
}

void HTMLDiagnosticSink::flagHardError() {
  _hadHardError = true;
}

void HTMLDiagnosticSink::push(std::string message) {
  if (_document != nullptr) {
    _document->errors.push_back(std::move(message));
  } else {
    _pending.push_back(std::move(message));
  }
}

}  // namespace pagx
