#pragma once
#include <memory>
namespace pag {

class PAGImageCache {
 public:
  static std::shared_ptr<PAGImageCache> Make(std::string path, int width, int height,
                                             int frameCount, bool needInit);
  bool savePixels(int frame, void* bitmapPixels);
  bool inflatePixels(int frame, void* bitmapPixels);
  bool isCached(int frame);
  bool isAllCached();
  void release();

 private:
  void* compressBuffer = nullptr;
  void* deCompressBuffer = nullptr;
  int deCompressBufferSize = 0;
  void* headerBuffer = nullptr;
  int frameCount = 0;
  int fd = -1;
};

}  // namespace pag
