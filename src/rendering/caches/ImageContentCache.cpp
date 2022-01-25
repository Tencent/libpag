/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
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

#include "ImageContentCache.h"
#include "rendering/graphics/Picture.h"

namespace pag {
class ImageBytesCache : public Cache {
public:
    static ImageBytesCache* Get(ImageBytes* imageBytes) {
        std::lock_guard<std::mutex> autoLock(imageBytes->locker);
        if (imageBytes->cache != nullptr) {
            return static_cast<ImageBytesCache*>(imageBytes->cache);
        }
        auto cache = new ImageBytesCache();
        auto fileBytes =
            Data::MakeWithoutCopy(imageBytes->fileBytes->data(), imageBytes->fileBytes->length());
        cache->image = Image::MakeFrom(std::move(fileBytes));
        auto picture = Picture::MakeFrom(imageBytes->uniqueID, cache->image);
        auto matrix = Matrix::MakeScale(1 / imageBytes->scaleFactor);
        matrix.postTranslate(static_cast<float>(-imageBytes->anchorX),
                             static_cast<float>(-imageBytes->anchorY));
        cache->graphic = Graphic::MakeCompose(picture, matrix);
        imageBytes->cache = cache;
        return cache;
    }

    std::shared_ptr<Image> image = nullptr;
    std::shared_ptr<Graphic> graphic = nullptr;
};

std::shared_ptr<Image> ImageContentCache::GetImage(ImageBytes* imageBytes) {
    return ImageBytesCache::Get(imageBytes)->image;
}

ImageContentCache::ImageContentCache(ImageLayer* layer) : ContentCache(layer) {
}

GraphicContent* ImageContentCache::createContent(Frame) const {
    auto imageBytes = static_cast<ImageLayer*>(layer)->imageBytes;
    auto graphic = ImageBytesCache::Get(imageBytes)->graphic;
    return new GraphicContent(graphic);
}
}  // namespace pag
