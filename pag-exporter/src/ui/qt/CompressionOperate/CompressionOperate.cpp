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
#include "CompressionOperate.h"
std::unique_ptr<CompressionOperate> CompressionOperate::MakeCompressionOperate(pagexporter::Context* context) {
  auto compressionOperate = new CompressionOperateImp(context);
  return std::unique_ptr<CompressionOperate>(compressionOperate);
}

CompressionOperateImp::~CompressionOperateImp() {
}

CompressionOperateImp::CompressionOperateImp(pagexporter::Context* context) {
  this->context = context;
}

bool CompressionOperateImp::showPanel() {
  //
  // to do
  //
  return true;
}