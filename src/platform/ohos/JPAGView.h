//
// Created on 2024/7/30.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".
#pragma once

#include <napi/native_api.h>
#include "pag/pag.h"
#include "rendering/PAGAnimator.h"

namespace pag {
class JPAGView : public PAGAnimator::Listener {
 public:
  static bool Init(napi_env env, napi_value exports);
  static inline std::string ClassName() {
    return "JPAGView";
  }

  explicit JPAGView(const std::string& id) : player(new PAGPlayer()), id(std::move(id)) {
  }
  virtual ~JPAGView() {
    napi_release_threadsafe_function(progressCallback, napi_tsfn_abort);
    napi_release_threadsafe_function(playingStateCallback, napi_tsfn_abort);
  }

  void onAnimationStart(PAGAnimator*) override;

  void onAnimationCancel(PAGAnimator*) override;

  void onAnimationEnd(PAGAnimator*) override;

  void onAnimationRepeat(PAGAnimator*) override;

  void onAnimationUpdate(PAGAnimator* animator) override;

  std::unique_ptr<PAGPlayer> player = nullptr;
  std::string id;
  std::shared_ptr<PAGAnimator> animator = nullptr;
  napi_threadsafe_function progressCallback;
  napi_threadsafe_function playingStateCallback;

 private:
  static napi_value Constructor(napi_env env, napi_callback_info info);
};
}  // namespace pag
