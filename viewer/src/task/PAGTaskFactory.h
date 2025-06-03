/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <QObject>
#include "PAGTask.h"
#include "pag/pag.h"

namespace pag {

class PAGTaskFactory : public QObject {
  Q_OBJECT
 public:
  Q_ENUMS(PAGTaskType)

  enum PAGTaskType { PAGTaskType_None, PAGTaskType_ExportPNG, PAGTaskType_ExportAPNG };

  Q_INVOKABLE PAGTask* createTask(PAGTaskType taskType, const QString& outPath,
                                  const QVariantMap& extraParams);

  void setPAGFile(const std::shared_ptr<PAGFile>& pagFile);

 private:
  PAGTask* task = nullptr;
  std::shared_ptr<PAGFile> pagFile = nullptr;
};

}  // namespace pag
