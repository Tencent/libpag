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

#include <QString>
#include <vector>

namespace pag {

class PAGTreeNode {
 public:
  explicit PAGTreeNode(PAGTreeNode* parent);
  ~PAGTreeNode();

  PAGTreeNode* getChild(int row) const;
  PAGTreeNode* getParent() const;
  int getRow() const;
  int getChildrenCount() const;
  int getColumnCount() const;
  QVariant getData(int column) const;
  QString getName() const;
  QString getValue() const;
  void setName(QString name);
  void setValue(QVariant value);
  void setValue(QString value);
  void setParent(PAGTreeNode* parent);
  void appendChild(std::unique_ptr<PAGTreeNode> child);
  void clear();

 private:
  QString name = "";
  QString value = "";
  PAGTreeNode* parent = nullptr;
  std::vector<std::unique_ptr<PAGTreeNode>> children = {};
};

}  //  namespace pag
