/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include "pagx/xml/XMLDOM.h"

namespace pagx {

/**
 * Collects the class rules declared by the <style> elements of an SVG document rooted at svgRoot:
 * direct <style> children of the root and direct <style> children of its root-level <defs>, in
 * document order. Only class selectors are kept; comma-separated selector groups register every
 * class they contain, and later rules for the same class append their declarations to earlier
 * ones with ";" so the later declarations win. The values of rules are declaration text (e.g.
 * "fill: red; transform: translate(10px,20px)").
 */
void CollectSvgClassRules(const std::shared_ptr<DOMNode>& svgRoot,
                          std::unordered_map<std::string, std::string>* rules);

/**
 * Concatenates the declaration text of every collected class rule matching node's class
 * attribute, in the attribute's class order, so parsing the result yields the same values as
 * applying each class rule in order. Returns an empty string when node declares no class or
 * nothing matches.
 */
std::string CollectSvgClassDeclarations(const std::unordered_map<std::string, std::string>& rules,
                                        const std::shared_ptr<DOMNode>& node);

}  // namespace pagx
