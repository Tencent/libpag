//
// Created on 2024/7/29.
//
// Node APIs are not fully supported. To solve the compilation error of the interface cannot be found,
// please include "napi/native_api.h".

#include <functional>
#include <mutex>
#include <native_vsync/native_vsync.h>
#include "rendering/utils/DisplayLink.h"
namespace pag {

class NativeDisplayLink : public DisplayLink {
 public:
  explicit NativeDisplayLink(std::function<void()> callback);
  ~NativeDisplayLink() override;

  void start() override;
  void stop() override;

 private:
  static void PAGVSyncCallback(long long timestamp, void* data);
  OH_NativeVSync* vSync = nullptr;
  std::function<void()> callback = nullptr;
  std::atomic<bool> playing = false;
};
}  // namespace pag
