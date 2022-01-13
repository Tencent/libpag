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

#pragma once

#include <type_traits>

namespace pag {
#define TGFX_MARK_AS_BITMASK_ENUM(LargestValue) TGFX_BITMASK_LARGEST_ENUMERATOR = LargestValue

/// Traits class to determine whether an enum has a
/// TGFX_BITMASK_LARGEST_ENUMERATOR enumerator.
template <typename E, typename Enable = void>
struct is_bitmask_enum : std::false_type {};

template <typename E>
struct is_bitmask_enum<E, std::enable_if_t<sizeof(E::TGFX_BITMASK_LARGEST_ENUMERATOR) >= 0>>
    : std::true_type {};

template <typename E>
std::underlying_type_t<E> BitMask() {
  // On overflow, NextPowerOf2 returns zero with the type uint64_t, so
  // subtracting 1 gives us the mask with all bits set, like we want.
  return NextPowerOf2(static_cast<std::underlying_type_t<E>>(E::TGFX_BITMASK_LARGEST_ENUMERATOR)) -
         1;
}

/// Check that Val is in range for E, and return Val cast to E's underlying
/// type.
template <typename E>
std::underlying_type_t<E> Underlying(E Val) {
  return static_cast<std::underlying_type_t<E>>(Val);
}

template <typename E, typename = std::enable_if_t<is_bitmask_enum<E>::value>>
E operator~(E Val) {
  return static_cast<E>(~Underlying(Val) & BitMask<E>());
}

template <typename E, typename = std::enable_if_t<is_bitmask_enum<E>::value>>
E operator|(E LHS, E RHS) {
  return static_cast<E>(Underlying(LHS) | Underlying(RHS));
}

template <typename E, typename = std::enable_if_t<is_bitmask_enum<E>::value>>
E operator&(E LHS, E RHS) {
  return static_cast<E>(Underlying(LHS) & Underlying(RHS));
}

template <typename E, typename = std::enable_if_t<is_bitmask_enum<E>::value>>
E operator^(E LHS, E RHS) {
  return static_cast<E>(Underlying(LHS) ^ Underlying(RHS));
}

// |=, &=, and ^= return a reference to LHS, to match the behavior of the
// operators on builtin types.

template <typename E, typename = std::enable_if_t<is_bitmask_enum<E>::value>>
E& operator|=(E& LHS, E RHS) {
  LHS = LHS | RHS;
  return LHS;
}

template <typename E, typename = std::enable_if_t<is_bitmask_enum<E>::value>>
E& operator&=(E& LHS, E RHS) {
  LHS = LHS & RHS;
  return LHS;
}

template <typename E, typename = std::enable_if_t<is_bitmask_enum<E>::value>>
E& operator^=(E& LHS, E RHS) {
  LHS = LHS ^ RHS;
  return LHS;
}
}  // namespace pag
