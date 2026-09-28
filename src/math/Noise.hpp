#pragma once

#include <cmath>
#include <cstdint>

namespace motioncab::math {

// Deterministic 1D gradient (Perlin-style) noise, output in about [-1, 1].
// Continuous and differentiable, so it can be sampled directly every frame
// without any smoothing pass.
//
// Periodic over kNoisePeriod cells, so a phase that runs for the whole
// session can be wrapped (WrapNoisePhase) without a jump. Left to grow, a
// float phase loses the precision its per-frame step needs after a few
// hours, and the motion turns jerky.
inline float Hash01(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dU;
  x ^= x >> 15;
  x *= 0x846ca68bU;
  x ^= x >> 16;
  return static_cast<float>(x & 0xFFFFFFu) / static_cast<float>(0xFFFFFFu);
}

inline constexpr uint32_t kNoisePeriod = 4096; // cells, a power of two

// Keeps a noise phase (in cells, >= 0) within one period. Held in double so
// the per-frame step never rounds away; cast to float only when sampled,
// where under kNoisePeriod float is precise to ~0.0005 cell.
inline double WrapNoisePhase(double phase) {
  return std::fmod(phase, static_cast<double>(kNoisePeriod));
}

inline float GradientNoise1D(float t, uint32_t seed) {
  const float floor_t = std::floor(t);
  const auto i = static_cast<uint32_t>(static_cast<int32_t>(floor_t));
  const float f = t - floor_t;
  // Random slope in [-1, 1] at each integer lattice point, repeating every
  // kNoisePeriod points.
  constexpr uint32_t kMask = kNoisePeriod - 1;
  const float g0 = Hash01((i & kMask) * 0x9E3779B1U + seed) * 2.0f - 1.0f;
  const float g1 =
      Hash01(((i + 1) & kMask) * 0x9E3779B1U + seed) * 2.0f - 1.0f;
  const float u = f * f * f * (f * (f * 6.0f - 15.0f) + 10.0f); // quintic fade
  const float n0 = g0 * f;
  const float n1 = g1 * (f - 1.0f);
  return (n0 + (n1 - n0) * u) * 2.0f;
}

} // namespace motioncab::math
