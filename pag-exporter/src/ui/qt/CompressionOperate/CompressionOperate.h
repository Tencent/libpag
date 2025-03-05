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
#ifndef COMPRESSIONOPERATE_H
#define COMPRESSIONOPERATE_H
#include "src/exports/PAGDataTypes.h"
#include <memory>

class CompressionOperate {
public:
  static std::unique_ptr<CompressionOperate> MakeCompressionOperate(pagexporter::Context* context);

  virtual ~CompressionOperate() = default;

  virtual bool showPanel() = 0;
};

class CompressionOperateImp : public CompressionOperate {
public:
  CompressionOperateImp(pagexporter::Context* context);
  ~CompressionOperateImp() override;

  bool showPanel() override;

private:
  pagexporter::Context* context = nullptr;
};
#endif //COMPRESSIONOPERATE_H
