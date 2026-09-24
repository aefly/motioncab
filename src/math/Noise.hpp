#pragma once

#include <cmath>
#include <cstdint>

namespace motioncab::math {

// Deterministic 1D gradient (Perlin-style) noise, output in about [-1, 1].
// Continuous and differentiable, so it can be sampled directly every frame
// without any smoothing pass.
inline float Hash01(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return static_cast<float>(x & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

inline float GradientNoise1D(float t, uint32_t seed) {
  const float floor_t = std::floor(t);
  const auto i = static_cast<int32_t>(floor_t);
  const float f = t - floor_t;
  // Random slope in [-1, 1] at each integer lattice point.
  const float g0 =
      Hash01(static_cast<uint32_t>(i) * 0x9E3779B1U + seed) * 2.0f - 1.0f;
  const float g1 =
      Hash01(static_cast<uint32_t>(i + 1) * 0x9E3779B1U + seed) * 2.0f - 1.0f;
  const float u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f); // quintic fade
  const float n0 = g0 * f;
  const float n1 = g1 * (f - 1.0f);
  return (n0 + (n1 - n0) * u) * 2.0f;
}

} // namespace motioncab::math
