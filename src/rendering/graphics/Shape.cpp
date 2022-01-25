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

#include "Shape.h"
#include "core/Canvas.h"
#include "pag/file.h"

namespace pag {
std::shared_ptr<Graphic> Shape::MakeFrom(const Path& path, Color color) {
    if (path.isEmpty()) {
        return nullptr;
    }
    auto fill = new SolidFill();
    fill->color = color;
    return std::shared_ptr<Graphic>(new Shape(path, fill));
}

std::shared_ptr<Graphic> Shape::MakeFrom(const Path& path, const GradientPaint& gradient) {
    if (path.isEmpty()) {
        return nullptr;
    }
    auto fill = new GradientFill();
    fill->gradient = gradient;
    return std::shared_ptr<Graphic>(new Shape(path, fill));
}

Shape::Shape(Path path, ShapeFill* fill) : path(std::move(path)), fill(fill) {
}

Shape::~Shape() {
    delete fill;
}

void Shape::measureBounds(Rect* bounds) const {
    *bounds = path.getBounds();
}

bool Shape::hitTest(RenderCache*, float x, float y) {
    return path.contains(x, y);
}

bool Shape::getPath(Path* result) const {
    if (fill->type() == ShapeFillType::Gradient) {
        return false;
    }
    result->addPath(path);
    return true;
}

void Shape::prepare(RenderCache*) const {
}

void Shape::draw(Canvas* canvas, RenderCache*) const {
    switch (fill->type()) {
    case ShapeFillType::Solid:
        canvas->drawPath(path, static_cast<SolidFill*>(fill)->color);
        break;
    case ShapeFillType::Gradient:
        canvas->drawPath(path, static_cast<GradientFill*>(fill)->gradient);
        break;
    }
}

}  // namespace pag