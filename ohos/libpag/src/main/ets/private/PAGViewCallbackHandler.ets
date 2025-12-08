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

import { PAGImageViewController } from '../PAGImageViewController'
import { PAGViewController } from '../PAGViewController'

export class PAGViewCallbackHandler {
  constructor(viewController: PAGViewController | PAGImageViewController) {
    this.weakViewController = new WeakRef(viewController);
  }

  onProgressUpdate = () => {
    let viewController = this.weakViewController.deref();
    if (viewController) {
      viewController.onAnimatorProgressUpdate();
    }
  }
  onStateChange = (state: number) => {
    let viewController = this.weakViewController.deref();
    if (viewController) {
      viewController.onAnimatorStateChange(state);
    }
  }
  private weakViewController: WeakRef<PAGViewController | PAGImageViewController>;
}