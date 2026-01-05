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

#include <QObject>
#include "task/profiling/PAGBenchmarkTask.h"

namespace pag {

class PAGBenchmarkModel : public QObject {
  Q_OBJECT
 public:
  explicit PAGBenchmarkModel(QObject* parent = nullptr);

  Q_INVOKABLE bool startBenchmarkOnTemplate(bool isAuto);
  Q_SIGNAL void benchmarkComplete(bool isAuto, QString templateAvgRenderingTime,
                                  QString templateFirstFrameRenderingTime);
  Q_SLOT void onBenchmarkOnTemplateFinished(int result, QString filePath);

 private:
  bool isAuto = false;
  QMap<QString, std::shared_ptr<PAGBenchmarkTask>> taskMap = {};
};

}  // namespace pag
