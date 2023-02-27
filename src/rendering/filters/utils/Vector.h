/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2023 THL A29 Limited, a Tencent company. All rights reserved.
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

#include <algorithm>  // std::min, std::max
#include <cassert>
#include <cmath>             // ceilf, floorf, truncf, roundf, sqrtf, etc.
#include <cstdint>           // intXX_t
#include <cstring>           // memcpy()
#include <initializer_list>  // std::initializer_list
#include <utility>           // std::index_sequence

#define VECTOR_USE_SIMD 1

#if VECTOR_USE_SIMD
#if defined(__SSE__) || defined(__AVX__) || defined(__AVX2__)
#include <immintrin.h>
#elif defined(__ARM_NEON)
#include <arm_neon.h>
#elif defined(__wasm_simd128__)
#include <wasm_simd128.h>
#endif
#endif

// To avoid ODR violations, all methods must be force-inlined...
#if defined(_MSC_VER)
#define VECTOR_ALWAYS_INLINE __forceinline
#else
#define VECTOR_ALWAYS_INLINE __attribute__((always_inline))
#endif

// ... and all standalone functions must be static.  Please use these helpers:
#define SI static inline
#define SIT             \
  template <typename T> \
  SI
#define SIN        \
  template <int N> \
  SI
#define SINT                   \
  template <int N, typename T> \
  SI
#define SINTU                                                              \
  template <int N, typename T, typename U,                                 \
            typename = std::enable_if_t<std::is_convertible<U, T>::value>> \
  SI

namespace pag {

template <int N, typename T>
struct alignas(N * sizeof(T)) Vec;

template <int... Ix, int N, typename T>
SI Vec<sizeof...(Ix), T> shuffle(const Vec<N, T>&);

template <typename D, typename S>
SI D bit_pun(const S&);

// All Vec have the same simple memory layout, the same as `T vec[N]`.
template <int N, typename T>
struct alignas(N * sizeof(T)) VecStorage {
  VECTOR_ALWAYS_INLINE VecStorage() = default;
  VECTOR_ALWAYS_INLINE VecStorage(T s) : lo(s), hi(s) {
  }

  Vec<N / 2, T> lo, hi;
};

template <typename T>
struct VecStorage<4, T> {
  VECTOR_ALWAYS_INLINE VecStorage() = default;
  VECTOR_ALWAYS_INLINE VecStorage(T s) : lo(s), hi(s) {
  }
  VECTOR_ALWAYS_INLINE VecStorage(T x, T y, T z, T w) : lo(x, y), hi(z, w) {
  }
  VECTOR_ALWAYS_INLINE VecStorage(Vec<2, T> xy, T z, T w) : lo(xy), hi(z, w) {
  }
  VECTOR_ALWAYS_INLINE VecStorage(T x, T y, Vec<2, T> zw) : lo(x, y), hi(zw) {
  }
  VECTOR_ALWAYS_INLINE VecStorage(Vec<2, T> xy, Vec<2, T> zw) : lo(xy), hi(zw) {
  }

  VECTOR_ALWAYS_INLINE Vec<2, T>& xy() {
    return lo;
  }
  VECTOR_ALWAYS_INLINE Vec<2, T>& zw() {
    return hi;
  }
  VECTOR_ALWAYS_INLINE T& x() {
    return lo.lo.val;
  }
  VECTOR_ALWAYS_INLINE T& y() {
    return lo.hi.val;
  }
  VECTOR_ALWAYS_INLINE T& z() {
    return hi.lo.val;
  }
  VECTOR_ALWAYS_INLINE T& w() {
    return hi.hi.val;
  }

  VECTOR_ALWAYS_INLINE Vec<2, T> xy() const {
    return lo;
  }
  VECTOR_ALWAYS_INLINE Vec<2, T> zw() const {
    return hi;
  }
  VECTOR_ALWAYS_INLINE T x() const {
    return lo.lo.val;
  }
  VECTOR_ALWAYS_INLINE T y() const {
    return lo.hi.val;
  }
  VECTOR_ALWAYS_INLINE T z() const {
    return hi.lo.val;
  }
  VECTOR_ALWAYS_INLINE T w() const {
    return hi.hi.val;
  }

  // Exchange-based swizzles. These should take 1 cycle on NEON and 3 (pipelined) cycles on SSE.
  VECTOR_ALWAYS_INLINE Vec<4, T> yxwz() const {
    return shuffle<1, 0, 3, 2>(bit_pun<Vec<4, T>>(*this));
  }
  VECTOR_ALWAYS_INLINE Vec<4, T> zwxy() const {
    return shuffle<2, 3, 0, 1>(bit_pun<Vec<4, T>>(*this));
  }

  Vec<2, T> lo, hi;
};

template <typename T>
struct VecStorage<2, T> {
  VECTOR_ALWAYS_INLINE VecStorage() = default;
  VECTOR_ALWAYS_INLINE VecStorage(T s) : lo(s), hi(s) {
  }
  VECTOR_ALWAYS_INLINE VecStorage(T x, T y) : lo(x), hi(y) {
  }

  VECTOR_ALWAYS_INLINE T& x() {
    return lo.val;
  }
  VECTOR_ALWAYS_INLINE T& y() {
    return hi.val;
  }

  VECTOR_ALWAYS_INLINE T x() const {
    return lo.val;
  }
  VECTOR_ALWAYS_INLINE T y() const {
    return hi.val;
  }

  // This exchange-based swizzle should take 1 cycle on NEON and 3 (pipelined) cycles on SSE.
  VECTOR_ALWAYS_INLINE Vec<2, T> yx() const {
    return shuffle<1, 0>(bit_pun<Vec<2, T>>(*this));
  }

  VECTOR_ALWAYS_INLINE Vec<4, T> xyxy() const {
    return Vec<4, T>(bit_pun<Vec<2, T>>(*this), bit_pun<Vec<2, T>>(*this));
  }

  Vec<1, T> lo, hi;
};

template <int N, typename T>
struct alignas(N * sizeof(T)) Vec : public VecStorage<N, T> {
  static_assert((N & (N - 1)) == 0, "N must be a power of 2.");
  static_assert(sizeof(T) >= alignof(T), "What kind of unusual T is this?");

  // Methods belong here in the class declaration of Vec only if:
  //   - they must be here, like constructors or operator[];
  //   - they'll definitely never want a specialized implementation.
  // Other operations on Vec should be defined outside the type.

  VECTOR_ALWAYS_INLINE Vec() = default;

  using VecStorage<N, T>::VecStorage;

  // NOTE: Vec{x} produces x000..., whereas Vec(x) produces xxxx.... since this constructor fills
  // unspecified lanes with 0s, whereas the single T constructor fills all lanes with the value.
  VECTOR_ALWAYS_INLINE Vec(std::initializer_list<T> xs) {
    T vals[N] = {0};
    memcpy(vals, xs.begin(), std::min(xs.size(), (size_t)N) * sizeof(T));

    this->lo = Vec<N / 2, T>::Load(vals + 0);
    this->hi = Vec<N / 2, T>::Load(vals + N / 2);
  }

  VECTOR_ALWAYS_INLINE T operator[](int i) const {
    return i < N / 2 ? this->lo[i] : this->hi[i - N / 2];
  }
  VECTOR_ALWAYS_INLINE T& operator[](int i) {
    return i < N / 2 ? this->lo[i] : this->hi[i - N / 2];
  }

  VECTOR_ALWAYS_INLINE static Vec Load(const void* ptr) {
    Vec v;
    memcpy(&v, ptr, sizeof(Vec));
    return v;
  }
  VECTOR_ALWAYS_INLINE void store(void* ptr) const {
    memcpy(ptr, this, sizeof(Vec));
  }
};

template <typename T>
struct Vec<1, T> {
  T val;

  VECTOR_ALWAYS_INLINE Vec() = default;

  Vec(T s) : val(s) {
  }

  VECTOR_ALWAYS_INLINE Vec(std::initializer_list<T> xs) : val(xs.size() ? *xs.begin() : 0) {
  }

  VECTOR_ALWAYS_INLINE T operator[](int) const {
    return val;
  }
  VECTOR_ALWAYS_INLINE T& operator[](int) {
    return val;
  }

  VECTOR_ALWAYS_INLINE static Vec Load(const void* ptr) {
    Vec v;
    memcpy(&v, ptr, sizeof(Vec));
    return v;
  }
  VECTOR_ALWAYS_INLINE void store(void* ptr) const {
    memcpy(ptr, this, sizeof(Vec));
  }
};

template <typename D, typename S>
SI D bit_pun(const S& s) {
  static_assert(sizeof(D) == sizeof(S));
  D d;
  memcpy(&d, &s, sizeof(D));
  return d;
}

// Translate from a value type T to its corresponding Mask, the result of a comparison.
template <typename T>
struct Mask {
  using type = T;
};
template <>
struct Mask<float> {
  using type = int32_t;
};
template <>
struct Mask<double> {
  using type = int64_t;
};
template <typename T>
using M = typename Mask<T>::type;

// Join two Vec<N,T> into one Vec<2N,T>.
//SINT Vec<2*N,T> join(const Vec<N,T>& lo, const Vec<N,T>& hi) {
//   Vec<2*N,T> v;
//   v.lo = lo;
//   v.hi = hi;
//   return v;
//}

// We have three strategies for implementing Vec operations:
//    1) lean on Clang/GCC vector extensions when available;
//    2) use map() to apply a scalar function lane-wise;
//    3) recurse on lo/hi to scalar portable implementations.
// We can slot in platform-specific implementations as overloads for particular Vec<N,T>,
// or often integrate them directly into the recursion of style 3), allowing fine control.

#if VECTOR_USE_SIMD && (defined(__clang__) || defined(__GNUC__))

// VExt<N,T> types have the same size as Vec<N,T> and support most operations directly.
#if defined(__clang__)
template <int N, typename T>
using VExt = T __attribute__((ext_vector_type(N)));

#elif defined(__GNUC__)
template <int N, typename T>
struct VExtHelper {
  typedef T __attribute__((vector_size(N * sizeof(T)))) type;
};

template <int N, typename T>
using VExt = typename VExtHelper<N, T>::type;

// For some reason some (new!) versions of GCC cannot seem to deduce N in the generic
// to_vec<N,T>() below for N=4 and T=float.  This workaround seems to help...
SI Vec<4, float> to_vec(VExt<4, float> v) {
  return bit_pun<Vec<4, float>>(v);
}
#endif

SINT VExt<N, T> to_vext(const Vec<N, T>& v) {
  return bit_pun<VExt<N, T>>(v);
}
SINT Vec<N, T> to_vec(const VExt<N, T>& v) {
  return bit_pun<Vec<N, T>>(v);
}

SINT Vec<N, T> operator+(const Vec<N, T>& x, const Vec<N, T>& y) {
  return to_vec<N, T>(to_vext(x) + to_vext(y));
}
SINT Vec<N, T> operator-(const Vec<N, T>& x, const Vec<N, T>& y) {
  return to_vec<N, T>(to_vext(x) - to_vext(y));
}
SINT Vec<N, T> operator*(const Vec<N, T>& x, const Vec<N, T>& y) {
  return to_vec<N, T>(to_vext(x) * to_vext(y));
}
SINT Vec<N, T> operator/(const Vec<N, T>& x, const Vec<N, T>& y) {
  return to_vec<N, T>(to_vext(x) / to_vext(y));
}

SINT Vec<N, T> operator^(const Vec<N, T>& x, const Vec<N, T>& y) {
  return to_vec<N, T>(to_vext(x) ^ to_vext(y));
}
SINT Vec<N, T> operator&(const Vec<N, T>& x, const Vec<N, T>& y) {
  return to_vec<N, T>(to_vext(x) & to_vext(y));
}
SINT Vec<N, T> operator|(const Vec<N, T>& x, const Vec<N, T>& y) {
  return to_vec<N, T>(to_vext(x) | to_vext(y));
}

SINT Vec<N, T> operator!(const Vec<N, T>& x) {
  return to_vec<N, T>(!to_vext(x));
}
SINT Vec<N, T> operator-(const Vec<N, T>& x) {
  return to_vec<N, T>(-to_vext(x));
}
SINT Vec<N, T> operator~(const Vec<N, T>& x) {
  return to_vec<N, T>(~to_vext(x));
}

SINT Vec<N, T> operator<<(const Vec<N, T>& x, int k) {
  return to_vec<N, T>(to_vext(x) << k);
}
SINT Vec<N, T> operator>>(const Vec<N, T>& x, int k) {
  return to_vec<N, T>(to_vext(x) >> k);
}

SINT Vec<N, M<T>> operator==(const Vec<N, T>& x, const Vec<N, T>& y) {
  return bit_pun<Vec<N, M<T>>>(to_vext(x) == to_vext(y));
}
SINT Vec<N, M<T>> operator!=(const Vec<N, T>& x, const Vec<N, T>& y) {
  return bit_pun<Vec<N, M<T>>>(to_vext(x) != to_vext(y));
}
SINT Vec<N, M<T>> operator<=(const Vec<N, T>& x, const Vec<N, T>& y) {
  return bit_pun<Vec<N, M<T>>>(to_vext(x) <= to_vext(y));
}
SINT Vec<N, M<T>> operator>=(const Vec<N, T>& x, const Vec<N, T>& y) {
  return bit_pun<Vec<N, M<T>>>(to_vext(x) >= to_vext(y));
}
SINT Vec<N, M<T>> operator<(const Vec<N, T>& x, const Vec<N, T>& y) {
  return bit_pun<Vec<N, M<T>>>(to_vext(x) < to_vext(y));
}
SINT Vec<N, M<T>> operator>(const Vec<N, T>& x, const Vec<N, T>& y) {
  return bit_pun<Vec<N, M<T>>>(to_vext(x) > to_vext(y));
}

#else

// N == 1 scalar implementations.
SIT Vec<1, T> operator+(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val + y.val;
}
SIT Vec<1, T> operator-(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val - y.val;
}
SIT Vec<1, T> operator*(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val * y.val;
}
SIT Vec<1, T> operator/(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val / y.val;
}

SIT Vec<1, T> operator^(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val ^ y.val;
}
SIT Vec<1, T> operator&(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val & y.val;
}
SIT Vec<1, T> operator|(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val | y.val;
}

SIT Vec<1, T> operator!(const Vec<1, T>& x) {
  return !x.val;
}
SIT Vec<1, T> operator-(const Vec<1, T>& x) {
  return -x.val;
}
SIT Vec<1, T> operator~(const Vec<1, T>& x) {
  return ~x.val;
}

SIT Vec<1, T> operator<<(const Vec<1, T>& x, int k) {
  return x.val << k;
}
SIT Vec<1, T> operator>>(const Vec<1, T>& x, int k) {
  return x.val >> k;
}

SIT Vec<1, M<T>> operator==(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val == y.val ? ~0 : 0;
}
SIT Vec<1, M<T>> operator!=(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val != y.val ? ~0 : 0;
}
SIT Vec<1, M<T>> operator<=(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val <= y.val ? ~0 : 0;
}
SIT Vec<1, M<T>> operator>=(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val >= y.val ? ~0 : 0;
}
SIT Vec<1, M<T>> operator<(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val < y.val ? ~0 : 0;
}
SIT Vec<1, M<T>> operator>(const Vec<1, T>& x, const Vec<1, T>& y) {
  return x.val > y.val ? ~0 : 0;
}

// Recurse on lo/hi down to N==1 scalar implementations.
SINT Vec<N, T> operator+(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo + y.lo, x.hi + y.hi);
}
SINT Vec<N, T> operator-(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo - y.lo, x.hi - y.hi);
}
SINT Vec<N, T> operator*(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo * y.lo, x.hi * y.hi);
}
SINT Vec<N, T> operator/(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo / y.lo, x.hi / y.hi);
}

SINT Vec<N, T> operator^(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo ^ y.lo, x.hi ^ y.hi);
}
SINT Vec<N, T> operator&(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo & y.lo, x.hi & y.hi);
}
SINT Vec<N, T> operator|(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo | y.lo, x.hi | y.hi);
}

SINT Vec<N, T> operator!(const Vec<N, T>& x) {
  return join(!x.lo, !x.hi);
}
SINT Vec<N, T> operator-(const Vec<N, T>& x) {
  return join(-x.lo, -x.hi);
}
SINT Vec<N, T> operator~(const Vec<N, T>& x) {
  return join(~x.lo, ~x.hi);
}

SINT Vec<N, T> operator<<(const Vec<N, T>& x, int k) {
  return join(x.lo << k, x.hi << k);
}
SINT Vec<N, T> operator>>(const Vec<N, T>& x, int k) {
  return join(x.lo >> k, x.hi >> k);
}

SINT Vec<N, M<T>> operator==(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo == y.lo, x.hi == y.hi);
}
SINT Vec<N, M<T>> operator!=(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo != y.lo, x.hi != y.hi);
}
SINT Vec<N, M<T>> operator<=(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo <= y.lo, x.hi <= y.hi);
}
SINT Vec<N, M<T>> operator>=(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo >= y.lo, x.hi >= y.hi);
}
SINT Vec<N, M<T>> operator<(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo < y.lo, x.hi < y.hi);
}
SINT Vec<N, M<T>> operator>(const Vec<N, T>& x, const Vec<N, T>& y) {
  return join(x.lo > y.lo, x.hi > y.hi);
}
#endif

// Scalar/vector operations splat the scalar to a vector.
SINTU Vec<N, T> operator+(U x, const Vec<N, T>& y) {
  return Vec<N, T>(x) + y;
}
SINTU Vec<N, T> operator-(U x, const Vec<N, T>& y) {
  return Vec<N, T>(x) - y;
}
SINTU Vec<N, T> operator*(U x, const Vec<N, T>& y) {
  return Vec<N, T>(x) * y;
}
SINTU Vec<N, T> operator/(U x, const Vec<N, T>& y) {
  return Vec<N, T>(x) / y;
}

SINTU Vec<N, T> operator+(const Vec<N, T>& x, U y) {
  return x + Vec<N, T>(y);
}
SINTU Vec<N, T> operator-(const Vec<N, T>& x, U y) {
  return x - Vec<N, T>(y);
}
SINTU Vec<N, T> operator*(const Vec<N, T>& x, U y) {
  return x * Vec<N, T>(y);
}
SINTU Vec<N, T> operator/(const Vec<N, T>& x, U y) {
  return x / Vec<N, T>(y);
}

// Some operations we want are not expressible with Clang/GCC vector extensions.

// Clang can reason about naive_if_then_else() and optimize through it better
// than if_then_else(), so it's sometimes useful to call it directly when we
// think an entire expression should optimize away, e.g. min()/max().
SINT Vec<N, T> naive_if_then_else(const Vec<N, M<T>>& cond, const Vec<N, T>& t,
                                  const Vec<N, T>& e) {
  return bit_pun<Vec<N, T>>((cond & bit_pun<Vec<N, M<T>>>(t)) | (~cond & bit_pun<Vec<N, M<T>>>(e)));
}

SIT Vec<1, T> if_then_else(const Vec<1, M<T>>& cond, const Vec<1, T>& t, const Vec<1, T>& e) {
  // In practice this scalar implementation is unlikely to be used.  See next if_then_else().
  return bit_pun<Vec<1, T>>((cond & bit_pun<Vec<1, M<T>>>(t)) | (~cond & bit_pun<Vec<1, M<T>>>(e)));
}
SINT Vec<N, T> if_then_else(const Vec<N, M<T>>& cond, const Vec<N, T>& t, const Vec<N, T>& e) {
  // Specializations inline here so they can generalize what types the apply to.
#if VECTOR_USE_SIMD && defined(__AVX2__)
  if constexpr (N * sizeof(T) == 32) {
    return bit_pun<Vec<N, T>>(
        _mm256_blendv_epi8(bit_pun<__m256i>(e), bit_pun<__m256i>(t), bit_pun<__m256i>(cond)));
  }
#endif
#if VECTOR_USE_SIMD && defined(__SSE4_1__)
  if constexpr (N * sizeof(T) == 16) {
    return bit_pun<Vec<N, T>>(
        _mm_blendv_epi8(bit_pun<__m128i>(e), bit_pun<__m128i>(t), bit_pun<__m128i>(cond)));
  }
#endif
#if VECTOR_USE_SIMD && defined(__ARM_NEON)
  if constexpr (N * sizeof(T) == 16) {
    return bit_pun<Vec<N, T>>(
        vbslq_u8(bit_pun<uint8x16_t>(cond), bit_pun<uint8x16_t>(t), bit_pun<uint8x16_t>(e)));
  }
#endif
  // Recurse for large vectors to try to hit the specializations above.
  if constexpr (N * sizeof(T) > 16) {
    return join(if_then_else(cond.lo, t.lo, e.lo), if_then_else(cond.hi, t.hi, e.hi));
  }
  // This default can lead to better code than the recursing onto scalars.
  return naive_if_then_else(cond, t, e);
}

SINT Vec<N, T> min(const Vec<N, T>& x, const Vec<N, T>& y) {
  return naive_if_then_else(y < x, y, x);
}
SINT Vec<N, T> max(const Vec<N, T>& x, const Vec<N, T>& y) {
  return naive_if_then_else(x < y, y, x);
}

// Shuffle values from a vector pretty arbitrarily:
//    skvx::Vec<4,float> rgba = {R,G,B,A};
//    shuffle<2,1,0,3>        (rgba) ~> {B,G,R,A}
//    shuffle<2,1>            (rgba) ~> {B,G}
//    shuffle<2,1,2,1,2,1,2,1>(rgba) ~> {B,G,B,G,B,G,B,G}
//    shuffle<3,3,3,3>        (rgba) ~> {A,A,A,A}
// The only real restriction is that the output also be a legal N=power-of-two sknx::Vec.
template <int... Ix, int N, typename T>
SI Vec<sizeof...(Ix), T> shuffle(const Vec<N, T>& x) {
#if SKVX_USE_SIMD && defined(__clang__)
  // TODO: can we just always use { x[Ix]... }?
  return to_vec<sizeof...(Ix), T>(__builtin_shufflevector(to_vext(x), to_vext(x), Ix...));
#else
  return {x[Ix]...};
#endif
}

using float2 = Vec<2, float>;
using float4 = Vec<4, float>;
}  // namespace pag

#undef SINTU
#undef SINT
#undef SIN
#undef SIT
#undef SI
#undef VECTOR_ALWAYS_INLINE
#undef VECTOR_USE_SIMD
