///////////////////////////////////////////////////////////////////////////////////////////////////
//
//  The MIT License (MIT)
//
//  Copyright (c) 2016-present, Tencent. All rights reserved.
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy of this software
//  and associated documentation files (the "Software"), to deal in the Software without
//  restriction, including without limitation the rights to use, copy, modify, merge, publish,
//  distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
//
//      The above copyright notice and this permission notice shall be included in all copies or
//      substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
//  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
//  DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
///////////////////////////////////////////////////////////////////////////////////////////////////

#include "platform/ReportData.h"

#include <string>
#include <unordered_map>

#import"InternalReporter.h"

namespace pag {
#define SDKBeaconKey "0DOU0WKP4I47KH3I"

void OnReportData(std::unordered_map<std::string, std::string> &reportMap) {
  NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];
  for (auto it = reportMap.begin(); it != reportMap.end(); ++it) {
    NSString *key = [NSString stringWithUTF8String:it->first.c_str()];
    NSString *value = [NSString stringWithUTF8String:it->second.c_str()];
    dictionary[key] = value;
  }
  [[InternalReporter shareInstance]
        reportData:@"pag_monitor"
         reportKey:[NSString stringWithUTF8String:SDKBeaconKey]
            params:dictionary];

}
}  // namespace pag
