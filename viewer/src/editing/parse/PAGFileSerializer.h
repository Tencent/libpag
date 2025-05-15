/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "editing/parse/PAGTreeNode.h"
#include "pag/file.h"
#include "rttr/type.h"

namespace pag {

class PAGFileSerializer {
 public:
  static void serialize(std::shared_ptr<File>& file, PAGTreeNode* node);
  static void serializeInstance(const rttr::instance& item, PAGTreeNode* node);
  static bool serializeVariant(const rttr::variant& value, PAGTreeNode* node);
  static void serializeSequentialContainer(const rttr::variant_sequential_view& view,
                                           PAGTreeNode* node);
  static void serializeAssociativeContainer(const rttr::variant_associative_view& view,
                                            PAGTreeNode* node);
  static QString transformNumberToQString(const rttr::type& type, const rttr::variant& value);
  static QString transformEnumToQString(const rttr::type& type, const rttr::variant& value);
};

}  //  namespace pag