#pragma once

// Default values for the "Reset to Defaults" button in the custom
// MotionCab window (SettingsWindow.cpp). Must be kept in sync with the
// JSON defaults declared in Manifest.cpp's BuildManifest: the SDK has
// no way to read the manifest's own defaults back out at runtime, so
// these are a deliberate copy.

namespace motioncab::defaults {

constexpr float kHeadMotionSwayStrength = 0.2f;
constexpr float kHeadMotionTiltStrength = 0.15f;
constexpr float kHeadMotionSmoothing = 0.2f;

constexpr float kSteeringCameraRotationAmount = 30.0f;
constexpr float kSteeringCameraSmoothing = 0.5f;
constexpr float kSteeringCameraReactionDelay = 0.2f;

constexpr float kIdleBreathingVerticalAmount = 0.002f;
constexpr float kIdleBreathingHeadNodAmount = 0.15f;
constexpr float kIdleBreathingRate = 15.0f;
constexpr float kIdleBreathingFadeStart = 30.0f;
constexpr float kIdleBreathingFadeEnd = 50.0f;

constexpr float kSuspensionVerticalStrength = 1.0f;
constexpr float kSuspensionReactivity = 0.25f;
constexpr float kSuspensionGradeStrength = 0.5f;

constexpr float kEngineVibrationIntensity = 1.0f;

constexpr float kMirrorCheckLookAngle = 28.0f;
constexpr float kMirrorCheckPitchOffset = 3.0f;
constexpr float kMirrorCheckSmoothing = 0.35f;
constexpr bool kMirrorCheckRequireStationary = true;
constexpr bool kMirrorCheckIgnoreAfterMovingSignal = true;

constexpr float kManualLookLookAngle = 45.0f;
constexpr float kManualLookSmoothing = 0.35f;
constexpr bool kManualLookToggleMode = false;

constexpr float kRoadIrregularityIntensity = 1.0f;
constexpr float kRoadIrregularityReactivity = 0.05f;

constexpr float kSpeedShakeIntensity = 1.0f;
constexpr float kSpeedShakeSmoothing = 0.30f;
constexpr float kSpeedShakeRotation = 1.0f;
constexpr float kSpeedShakeVertical = 1.0f;
constexpr float kSpeedShakeRoughness = 0.5f;

constexpr float kBodyDynamicsLeanStrength = 1.0f;
constexpr float kBodyDynamicsNodStrength = 1.0f;
constexpr float kBodyDynamicsSmoothing = 0.25f;

constexpr float kEngineStartStopIntensity = 0.1f;
constexpr float kEngineStartStopDuration = 1.0f;

constexpr float kManualZoomZoomLevel = 40.0f;
constexpr float kManualZoomSmoothing = 0.25f;

} // namespace motioncab::defaults
