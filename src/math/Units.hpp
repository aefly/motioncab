#pragma once

#include <cmath>
#include <numbers>

namespace motioncab::math {

inline constexpr float kPi = std::numbers::pi_v<float>;
inline constexpr float kTwoPi = 2.0f * kPi;
inline constexpr float kDegToRad = kPi / 180.0f;
inline constexpr float kMsToKmh = 3.6f;

// Unlike SmootherStep, `t` isn't clamped: keep it in [0, 1].
inline float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

// Zero speed and acceleration at both ends, so a move neither starts nor
// stops with a jolt.
inline float SmootherStep(float t) {
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// fmod rather than subtracting 2*pi once: a dt spike (load hitch, alt-tab,
// Quick Job reload) can push a phase several turns ahead in one frame, and
// std::sin loses precision on a phase left that large.
inline float WrapPhase(float phase_rad) { return std::fmod(phase_rad, kTwoPi); }

} // namespace motioncab::math
