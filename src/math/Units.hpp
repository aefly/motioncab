#pragma once

#include <cmath>
#include <numbers>

namespace motioncab::math {

inline constexpr float kPi = std::numbers::pi_v<float>;
inline constexpr float kTwoPi = 2.0f * kPi;
inline constexpr float kDegToRad = kPi / 180.0f;
inline constexpr float kMsToKmh = 3.6f;

// 0 at t = 0, 1 at t = 1, with zero slope at both ends. `t` must already be
// in [0, 1].
inline float SmoothStep(float t) { return t * t * (3.0f - 2.0f * t); }

// 0 to 1 with zero speed and acceleration at both ends (quintic), so a move
// neither starts nor stops with a jolt. `t` is clamped to [0, 1].
inline float SmootherStep(float t) {
  t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }

// Brings an oscillator phase back into [0, 2*pi). fmod rather than a single
// subtraction: a large dt spike (load hitch, alt-tab, a Quick Job cancel
// reload) can advance a phase by several full turns in one frame, and
// subtracting only one would leave it large enough for std::sin to lose
// precision for the next few frames.
inline float WrapPhase(float phase_rad) { return std::fmod(phase_rad, kTwoPi); }

} // namespace motioncab::math
