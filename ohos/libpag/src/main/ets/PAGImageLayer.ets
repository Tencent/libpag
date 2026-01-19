/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2024 Tencent. All rights reserved.
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

import { JPAGImageLayer, JPAGLayer } from 'libpag.so';
import { PAGImage } from './PAGImage';
import { PAGLayer } from './PAGLayer';
import { PAGVideoRange } from './PAGVideoRange';

export class PAGImageLayer extends PAGLayer {
  constructor(nativeLayer: JPAGLayer) {
    super(nativeLayer);
  }

  static Make(width: number, height: number, duration: number): PAGImageLayer | null {
    let nativeImageLayer = JPAGImageLayer.Make(width, height, duration);
    if (nativeImageLayer == null) {
      return null;
    } else {
      return new PAGImageLayer(nativeImageLayer);
    }
  }

  contentDuration(): number {
    return this.getNativeImageLayer().contentDuration();
  }

  getVideoRanges(): Array<PAGVideoRange> {
    return this.getNativeImageLayer().getVideoRanges() as Array<PAGVideoRange>;
  }

  setImage(image: PAGImage) {
    this.getNativeImageLayer().setImage(image.nativeImage);
  }

  imageBytes(): ArrayBuffer | null {
    return this.getNativeImageLayer().imageBytes();
  }

  getNativeImageLayer(): JPAGImageLayer {
    return this.nativeLayer as JPAGImageLayer;
  }


}
