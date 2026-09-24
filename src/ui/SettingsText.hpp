#pragma once

// Single source of truth for setting descriptions shown both as the
// native settings UI's tooltip (Manifest.cpp's Meta_AddCustomSetting
// descKey) and the custom MotionCab window's tooltip (SettingsWindow.cpp).
// The SDK has no way to read metadata back out of the manifest at
// runtime, so this header is what keeps the two in sync instead.

namespace motioncab::tip {

// Shared by every effect's "Enabled" toggle, native and custom alike.
constexpr const char *kEnabled = "Turn this effect on or off.";

constexpr const char *kHeadMotionSwayStrength =
    "How much your head gets pushed around when you speed up, brake, or "
    "take a turn.";
constexpr const char *kHeadMotionTiltStrength =
    "How much your head tilts when the cab leans or rocks.";
constexpr const char *kHeadMotionSmoothing =
    "How quickly your head settles into place. Higher = slower and "
    "smoother, lower = snappier.";

constexpr const char *kSteeringCameraRotationAmount =
    "How far the camera turns when you crank the wheel all the way.";
constexpr const char *kSteeringCameraSmoothing =
    "How smooth the camera turn feels once it gets going. Higher = "
    "smoother and slower.";
constexpr const char *kSteeringCameraReactionDelay =
    "How long the camera waits after you turn the wheel before it starts "
    "moving.";

constexpr const char *kIdleBreathingVerticalAmount =
    "How far the camera rises and falls with each breath.";
constexpr const char *kIdleBreathingHeadNodAmount =
    "How much your head tips forward/back with each breath.";
constexpr const char *kIdleBreathingRate = "How fast the breathing happens.";
constexpr const char *kIdleBreathingFadeStart =
    "The speed at which this effect starts fading away as you speed up.";
constexpr const char *kIdleBreathingFadeEnd =
    "The speed at which this effect has completely faded away.";

constexpr const char *kSuspensionVerticalStrength =
    "How much bumps in the road bounce your head up and down.";
constexpr const char *kSuspensionReactivity =
    "How quickly your head catches up after a bump. Lower = snappier.";
constexpr const char *kSuspensionGradeStrength =
    "Tilts your head up on downhill stretches and down on uphill ones.";

constexpr const char *kEngineVibrationIntensity =
    "How strong the engine rumble feels.";

constexpr const char *kMirrorCheckLookAngle =
    "How far your head turns to check the mirror.";
constexpr const char *kMirrorCheckPitchOffset =
    "How much your head tilts down while checking the mirror.";
constexpr const char *kMirrorCheckSmoothing =
    "How smooth the look-and-return motion is.";
constexpr const char *kMirrorCheckRequireStationary =
    "Only check the mirror when the truck is stopped.";
constexpr const char *kMirrorCheckIgnoreAfterMovingSignal =
    "Don't check the mirror for a turn signal you switched on while "
    "already driving, even after you stop.";

constexpr const char *kManualLookLookAngle =
    "How far your head turns when you look left or right.";
constexpr const char *kManualLookSmoothing =
    "How smooth the look-and-return motion is.";
constexpr const char *kManualLookToggleMode =
    "On: press once to look, press again to look back. Off: look while "
    "held, let go to look back.";

constexpr const char *kRoadIrregularityIntensity =
    "How strong the shake from rough road texture feels.";
constexpr const char *kRoadIrregularityReactivity =
    "How sharp and buzzy the road-texture shake feels. Lower = sharper.";

constexpr const char *kSpeedShakeIntensity =
    "Overall strength of the shake you feel from driving fast.";
constexpr const char *kSpeedShakeSmoothing =
    "How the shake feels over time. Higher = slower and smoother, lower "
    "= jumpier and more nervous.";

constexpr const char *kSpeedShakeRotation =
    "How much your view tilts/turns as part of the shake, versus just "
    "moving side to side. Set to 0 to keep only the side-to-side motion.";
constexpr const char *kSpeedShakeVertical =
    "How much up-and-down bounce is mixed into the shake.";
constexpr const char *kSpeedShakeRoughness =
    "How uneven the shake feels over time. 0 keeps it constant; higher "
    "gives bursts of calm and rough stretches.";

constexpr const char *kBodyDynamicsLeanStrength =
    "How much your head leans toward the outside of a turn.";
constexpr const char *kBodyDynamicsNodStrength =
    "How much your head dips forward when braking and pulls back when "
    "accelerating.";
constexpr const char *kBodyDynamicsSmoothing =
    "How quickly the lean and nod settle. Lower = snappier.";

constexpr const char *kEngineStartStopIntensity =
    "How strong the shudder feels when the engine starts or stops.";
constexpr const char *kEngineStartStopDuration =
    "How long the start-up shudder lasts. The stop shudder is shorter "
    "and gentler than this.";

constexpr const char *kManualZoomZoomLevel =
    "How zoomed in the view gets while holding the zoom key. Lower = "
    "more zoomed in.";
constexpr const char *kManualZoomSmoothing =
    "How smooth the zoom in/out feels.";

} // namespace motioncab::tip
