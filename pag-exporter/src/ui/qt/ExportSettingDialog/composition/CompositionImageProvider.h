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
#ifndef COMPOSITIONIMAGEPROVIDER_H
#define COMPOSITIONIMAGEPROVIDER_H
#include <QtQuick/QQuickImageProvider>

class CompositionImageProvider : public QQuickImageProvider {
public:
  CompositionImageProvider();
  ~CompositionImageProvider();
  QImage requestImage(const QString& id, QSize* size, const QSize& requestedSize);
  bool isCacheFrame(const QString& frameId);
  void cacheFrame(const QString& frameId, QImage* image);

private:
  std::map<QString, QImage*> snapshotCachedMap;
};

#endif //COMPOSITIONIMAGEPROVIDER_H
