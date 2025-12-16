/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 Tencent. All rights reserved.
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

#include "editing/serialize/PAGTreeNode.h"
#include "pag/file.h"
#include "rttr/type.h"

namespace pag::FileSerializer {

void Serialize(std::shared_ptr<File> file, PAGTreeNode* node);
void SerializeInstance(const rttr::instance& item, PAGTreeNode* node);
void SerializeVariant(const rttr::variant& value, PAGTreeNode* node);
void SerializeSequentialContainer(const rttr::variant_sequential_view& view, PAGTreeNode* node);
void SerializeAssociativeContainer(const rttr::variant_associative_view& view, PAGTreeNode* node);
QString TransformNumberToQString(const rttr::type& type, const rttr::variant& value);
QString TransformEnumToQString(const rttr::variant& value);

}  // namespace pag::FileSerializer
