/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 Tencent. All rights reserved.
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

#include "pag/c/pag_animator.h"
#include "pag_types_priv.h"
#include "rendering/PAGAnimator.h"

class PAGAnimatorListenerWrapper : public pag::PAGAnimator::Listener {
 public:
  static std::shared_ptr<PAGAnimatorListenerWrapper> Make(pag_animator_listener* listener,
                                                          void* user) {
    if (listener) {
      return std::shared_ptr<PAGAnimatorListenerWrapper>(
          new PAGAnimatorListenerWrapper(listener, user));
    }
    return nullptr;
  }

  void onAnimationStart(pag::PAGAnimator*) override {
    listener->on_animation_start(_animator, user);
  }

  void onAnimationEnd(pag::PAGAnimator*) override {
    listener->on_animation_end(_animator, user);
  }

  void onAnimationCancel(pag::PAGAnimator*) override {
    listener->on_animation_cancel(_animator, user);
  }

  void onAnimationRepeat(pag::PAGAnimator*) override {
    listener->on_animation_repeat(_animator, user);
  }

  void onAnimationUpdate(pag::PAGAnimator*) override {
    listener->on_animation_update(_animator, user);
  }

  void setAnimator(pag_animator* animator) {
    _animator = animator;
  }

 private:
  PAGAnimatorListenerWrapper(pag_animator_listener* listener, void* user)
      : listener(listener), user(user) {
  }

  pag_animator* _animator = nullptr;
  pag_animator_listener* listener = nullptr;
  void* user = nullptr;
};

pag_animator* pag_animator_create(pag_animator_listener* listener, void* user) {
  auto wrapper = PAGAnimatorListenerWrapper::Make(listener, user);
  if (wrapper == nullptr) {
    return nullptr;
  }
  if (auto animator = pag::PAGAnimator::MakeFrom(wrapper)) {
    auto* ret = new pag_animator();
    wrapper->setAnimator(ret);
    ret->p = std::move(animator);
    ret->listener = std::move(wrapper);
    return ret;
  }
  return nullptr;
}

bool pag_animator_is_sync(pag_animator* animator) {
  if (animator == nullptr) {
    return false;
  }
  return animator->p->isSync();
}

void pag_animator_set_sync(pag_animator* animator, bool sync) {
  if (animator == nullptr) {
    return;
  }
  animator->p->setSync(sync);
}

int64_t pag_animator_get_duration(pag_animator* animator) {
  if (animator == nullptr) {
    return 0;
  }
  return animator->p->duration();
}

void pag_animator_set_duration(pag_animator* animator, int64_t duration) {
  if (animator == nullptr) {
    return;
  }
  animator->p->setDuration(duration);
}

int pag_animator_get_repeat_count(pag_animator* animator) {
  if (animator == nullptr) {
    return 0;
  }
  return animator->p->repeatCount();
}

void pag_animator_set_repeat_count(pag_animator* animator, int repeatCount) {
  if (animator == nullptr) {
    return;
  }
  animator->p->setRepeatCount(repeatCount);
}

double pag_animator_get_progress(pag_animator* animator) {
  if (animator == nullptr) {
    return 0;
  }
  return animator->p->progress();
}

void pag_animator_set_progress(pag_animator* animator, double progress) {
  if (animator == nullptr) {
    return;
  }
  animator->p->setProgress(progress);
}

bool pag_animator_is_running(pag_animator* animator) {
  if (animator == nullptr) {
    return false;
  }
  return animator->p->isRunning();
}

void pag_animator_start(pag_animator* animator) {
  if (animator == nullptr) {
    return;
  }
  animator->p->start();
}

void pag_animator_cancel(pag_animator* animator) {
  if (animator == nullptr) {
    return;
  }
  animator->p->cancel();
}

void pag_animator_update(pag_animator* animator) {
  if (animator == nullptr) {
    return;
  }
  animator->p->update();
}
