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
#include "utils/AEResource.h"

namespace exporter {

class ExportTimeStretchModel : public QObject {
  Q_OBJECT
 public:
  explicit ExportTimeStretchModel(QObject* parent = nullptr);

  Q_PROPERTY(QString timeStretchMode READ getTimeStretchMode WRITE setTimeStretchMode NOTIFY
                 timeStretchModeChanged)
  Q_PROPERTY(int stretchStartTime READ getStretchStartTime WRITE setStretchStartTime NOTIFY
                 stretchStartTimeChanged)
  Q_PROPERTY(int stretchDuration READ getStretchDuration WRITE setStretchDuration NOTIFY
                 stretchDurationChanged)
  Q_PROPERTY(int duration READ getDuration NOTIFY durationChanged)
  Q_PROPERTY(int frameRate READ getFrameRate NOTIFY frameRateChanged)

  Q_INVOKABLE int getDuration() const;
  Q_INVOKABLE int getFrameRate() const;
  Q_INVOKABLE int getStretchStartTime() const;
  Q_INVOKABLE int getStretchDuration() const;
  Q_INVOKABLE QString getTimeStretchMode() const;
  Q_INVOKABLE QStringList getTimeStretchModes() const;

  Q_INVOKABLE void setTimeStretchMode(const QString& timeStretchMode);
  Q_INVOKABLE void setStretchStartTime(int stretchStartTime);
  Q_INVOKABLE void setStretchDuration(int stretchDuation);

  Q_SIGNAL void timeStretchModeChanged(const QString& timeStretchMode);
  Q_SIGNAL void stretchStartTimeChanged(int stretchStartTime);
  Q_SIGNAL void stretchDurationChanged(int stretchDuation);
  Q_SIGNAL void durationChanged(int duration);
  Q_SIGNAL void frameRateChanged(int frameRate);

  void setFrameRate(float frameRate);
  void setDuration(pag::Frame duration);
  void setAEResource(std::shared_ptr<AEResource> resource);

 private:
  float frameRate = 24.0f;
  pag::Frame duration = 0;
  std::shared_ptr<AEResource> resource = nullptr;
};

}  // namespace exporter
