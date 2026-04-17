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

#include "PAGTree.h"
#include "editing/serialize/PAGFileSerializer.h"

namespace pag {

void PAGTree::setFile(std::shared_ptr<File> file) {
  this->file = std::move(file);
  this->pagxDocument = nullptr;
}

void PAGTree::setPAGXDocument(std::shared_ptr<pagx::PAGXDocument> document) {
  this->pagxDocument = std::move(document);
  this->file = nullptr;
}

PAGTreeNode* PAGTree::getRootNode() {
  return rootNode.get();
}

void PAGTree::buildTree() {
  rootNode = nullptr;

  if (file != nullptr) {
    rootNode = std::make_unique<PAGTreeNode>(nullptr);
    rootNode->setName("file");
    FileSerializer::Serialize(file, rootNode.get());
  } else if (pagxDocument != nullptr) {
    // TODO: Implement PAGX document serialization
    rootNode = std::make_unique<PAGTreeNode>(nullptr);
    rootNode->setName("document");
    rootNode->setValue("PAGXDocument");
  }
}

}  // namespace pag
