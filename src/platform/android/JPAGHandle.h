#pragma once
#include <memory>

namespace pag {
template <typename T>
class JPAGHandle {
 public:
  explicit JPAGHandle(std::shared_ptr<T> nativeHandle) : nativeHandle(nativeHandle) {
  }

  std::shared_ptr<T> get() {
    return nativeHandle;
  }

  void reset() {
    nativeHandle = nullptr;
  }

 private:
  std::shared_ptr<T> nativeHandle;
};
}  // namespace pag