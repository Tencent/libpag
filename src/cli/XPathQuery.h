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

#include <libxml/parser.h>
#include <string>
#include <vector>
#include "pagx/PAGXDocument.h"

namespace pagx::cli {

/**
 * Evaluates an XPath expression on the XML document and returns matching pagx::Layer nodes. The
 * function maps XML nodes back to the PAGXDocument's layer tree by computing positional paths.
 * Non-Layer nodes matched by the expression are skipped with a warning on stderr.
 */
std::vector<const Layer*> EvaluateXPath(xmlDocPtr xmlDoc, const std::string& xpathExpr,
                                        const PAGXDocument* document);

/**
 * Non-const overload that returns mutable Layer pointers. Delegates to the const overload and
 * centralizes the const_cast, since PAGXDocument::layers stores non-const Layer pointers.
 */
std::vector<Layer*> EvaluateXPath(xmlDocPtr xmlDoc, const std::string& xpathExpr,
                                  PAGXDocument* document);

/**
 * Evaluates an XPath expression and returns exactly one matching Layer. Prints an error and returns
 * nullptr if zero or more than one Layer is matched.
 */
const Layer* EvaluateSingleXPath(xmlDocPtr xmlDoc, const std::string& xpathExpr,
                                 const PAGXDocument* document, const std::string& commandName);

}  // namespace pagx::cli
