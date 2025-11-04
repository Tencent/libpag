/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 Tencent. All rights reserved.
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

#include "base/utils/TGFXCast.h"
#include "pag/types.h"

namespace pag {

void Rect::scale(float scaleX, float scaleY) {
  ToTGFX(this)->scale(scaleX, scaleY);
}

bool Rect::setBounds(const Point pts[], int count) {
  return ToTGFX(this)->setBounds(ToTGFX(pts), count);
}

bool Rect::intersect(float l, float t, float r, float b) {
  return ToTGFX(this)->intersect(l, t, r, b);
}

bool Rect::intersect(const Rect& a, const Rect& b) {
  return ToTGFX(this)->intersect(ToTGFX(a), ToTGFX(b));
}

void Rect::join(float l, float t, float r, float b) {
  ToTGFX(this)->join(l, t, r, b);
}

bool Rect::intersects(float l, float t, float r, float b) const {
  return intersects(MakeLTRB(l, t, r, b));
}

bool Rect::intersects(const Rect& r) const {
  return tgfx::Rect::Intersects(*ToTGFX(this), ToTGFX(r));
}

bool Rect::Intersects(const Rect& a, const Rect& b) {
  return tgfx::Rect::Intersects(ToTGFX(a), ToTGFX(b));
}

void Rect::roundOut() {
  ToTGFX(this)->roundOut();
}

void Rect::round() {
  ToTGFX(this)->round();
}

void Rect::sort() {
  ToTGFX(this)->sort();
}

Rect Rect::makeSorted() const {
  return ToPAG(ToTGFX(this)->makeSorted());
}
}  // namespace pag
