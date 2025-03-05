//////////////////////////////////////////////////////////////////////////////////////
//
//  The MIT License (MIT)
//
//  Copyright (c) 2017-present, cyder.org
//  All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of
//  this software and associated documentation files (the "Software"), to deal in the
//  Software without restriction, including without limitation the rights to use, copy,
//  modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
//  and to permit persons to whom the Software is furnished to do so, subject to the
//  following conditions:
//
//      The above copyright notice and this permission notice shall be included in all
//      copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
//  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
//  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
//  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//////////////////////////////////////////////////////////////////////////////////////

#ifndef CYDER_STRINGUTIL_H
#define CYDER_STRINGUTIL_H

#include <AEGP_SuiteHandler.h>
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "src/utils/Context.h"

class StringUtil {
 public:
  static std::vector<std::string> Split(const std::string& text, const std::string& separator);

  static std::string ReplaceAll(const std::string& text, const std::string& from,
                                const std::string& to);

  static std::string ToLowerCase(const std::string& text);

  static std::string ToUpperCase(const std::string& text);

  static std::string DeleteLastSpace(const std::string& text);

  static std::string ToString(const AEGP_SuiteHandler& suites, AEGP_MemHandle memHandle);

  static bool IsSuffixContainsSequenceSuffix(const std::string& text,
                                             pagexporter::Context& context);
};

#endif  //CYDER_STRINGUTIL_H
