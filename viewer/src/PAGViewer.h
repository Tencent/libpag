/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include <QApplication>
#include "maintenance/PAGCheckUpdateModel.h"
#include "rendering/PAGWindow.h"

namespace pag {

class PAGViewer : public QApplication {
  Q_OBJECT
 public:
  PAGViewer(int& argc, char** argv);

  bool event(QEvent* event) override;
  void openFile(QString path);
  PAGCheckUpdateModel* getCheckUpdateModel();

  Q_SLOT void onWindowDestroyed(PAGWindow* window);

 private:
  std::unique_ptr<PAGCheckUpdateModel> checkUpdateModel = nullptr;
};

}  // namespace pag
