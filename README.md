<p align="center">
  <img src="./.img/logo.png" alt="Logo" width="128" />
</p>

<h1 align="center">MotionCab</h1>

<p align="center"><strong>Dynamic Head-Motion for ETS2/ATS</strong></p>

<!-- markdownlint-disable MD013 -->
<p align="center">
  <img alt="GitHub License" src="https://img.shields.io/github/license/aefly/motioncab?style=flat&logo=github&logoColor=white&labelColor=black&color=%23B82728" />
  <img alt="GitHub Actions Workflow Status" src="https://img.shields.io/github/actions/workflow/status/aefly/motioncab/build.yaml?style=flat&logo=githubactions&logoColor=white&labelColor=black&color=%23B82728" />
  <img alt="GitHub Release" src="https://img.shields.io/github/v/release/aefly/motioncab?sort=semver&display_name=release&style=flat&logo=semver&logoColor=white&labelColor=black&color=%23B82728" />
</p>
<!-- markdownlint-enable MD013 -->

<p align="center">
  <a href="#overview">Overview</a> •
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#controls">Controls</a> •
  <a href="#configuration">Configuration</a> •
  <a href="#building">Building</a> •
  <a href="#contributing">Contributing</a> •
  <a href="#license">License</a>
</p>

## Overview

MotionCab is a plugin for Euro Truck Simulator 2 and American Truck
Simulator that adds realistic dynamic head-motion camera effects for a
next-level driving experience.

It's built on the [SPF Framework][spf-framework], which handles telemetry,
camera control, keybind rebinding, and the in-game settings UI.

## Features

- **Head Motion** — inertial head sway/tilt under acceleration, braking,
  and cornering
- **Steering Camera** — smoothed camera yaw following the steering wheel
- **Idle Breathing** — subtle breathing motion that fades out with speed
- **Suspension** — camera follows real per-wheel suspension movement, with
  a grade-follow response to uphill/downhill slope
- **Engine Vibration** — RPM-scaled engine buzz
- **Mirror Check** — looks toward the mirror on turn signal
- **Manual Look** — smooth look-left/look-right
- **Road Irregularity** — chatter based on the ground material under the
  wheels
- **Speed Shake** — speed-driven body sway with slow, bounce, and fine
  vibration bands
- **Body Dynamics** — head leans outward in corners and nods forward when
  braking
- **Engine Start/Stop** — mechanical shudder when the engine catches or
  dies
- **Manual Zoom** — smooth zoom effect
- **Profiles** — create, save, and switch between named presets

Every effect above is independently customizable and toggleable, and only
active while in the interior (cabin) camera view.

## Installation

1. Install the [SPF Framework][spf-framework-download] in your game's
   `bin/win_x64/plugins/` folder.
2. Install [MotionCab][motioncab-web] in your game's
   `bin/win_x64/plugins/spfPlugins/` folder.
3. Start your game and enable **MotionCab** in SPF's plugin manager.

## Controls

| Action        | Default key | Behavior                                                   |
| ------------- | ----------- | ---------------------------------------------------------- |
| Look Left     | Numpad /    | Manual Look — hold or toggle to look left                  |
| Look Right    | Numpad *    | Manual Look — hold or toggle to look right                 |
| Zoom          | Numpad -    | Manual Zoom — hold to zoom in, release to return to normal |
| Toggle Window | F9          | Shows/hides the MotionCab Quick Settings window            |

All keys are fully rebindable in SPF's in-game Keybinds drawer.

## Configuration

All parameters are tuned live either through SPF's native in-game settings
window, or through MotionCab's own **Quick Settings** window, which mirrors
the same settings and adds a few conveniences: right-click any slider to reset
it to default, and the **Settings** tab has a button to reset every effect at once.
Values are persisted by the framework to `plugins/spfPlugins/MotionCab/config/settings.json`.

| Setting                      | Default | Description                                                                                                                          |
| ---------------------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| **Head Motion**              |         |                                                                                                                                      |
| `sway_strength`              | 0.2     | How much your head gets pushed around when you speed up, brake, or take a turn.                                                      |
| `tilt_strength`              | 0.15    | How much your head tilts when the cab leans or rocks.                                                                                |
| `smoothing_time`             | 0.2     | How quickly your head settles into place. Higher = slower and smoother, lower = snappier.                                            |
| **Steering Camera**          |         |                                                                                                                                      |
| `rotation_factor_deg`        | 30.0    | How far the camera turns when you crank the wheel all the way.                                                                       |
| `smoothing_time`             | 0.5     | How smooth the camera turn feels once it gets going. Higher = smoother and slower.                                                   |
| `delay_seconds`              | 0.2     | How long the camera waits after you turn the wheel before it starts moving.                                                          |
| **Idle Breathing**           |         |                                                                                                                                      |
| `vertical_amplitude`         | 0.002   | How far the camera rises and falls with each breath.                                                                                 |
| `pitch_amplitude_deg`        | 0.15    | How much your head tips forward/back with each breath.                                                                               |
| `breathing_rate_bpm`         | 15.0    | How fast the breathing happens.                                                                                                      |
| `fade_start_kmh`             | 30.0    | The speed at which this effect starts fading away as you speed up.                                                                   |
| `fade_end_kmh`               | 50.0    | The speed at which this effect has completely faded away.                                                                            |
| **Suspension**               |         |                                                                                                                                      |
| `vertical_strength`          | 1.0     | How much bumps in the road bounce your head up and down.                                                                             |
| `reactivity`                 | 0.25    | How quickly your head catches up after a bump. Lower = snappier.                                                                     |
| `grade_strength`             | 0.5     | Tilts your head up on downhill stretches and down on uphill ones.                                                                    |
| **Engine Vibration**         |         |                                                                                                                                      |
| `intensity`                  | 1.0     | How strong the engine rumble feels.                                                                                                  |
| **Mirror Check**             |         |                                                                                                                                      |
| `look_angle_deg`             | 28.0    | How far your head turns to check the mirror.                                                                                         |
| `pitch_offset_deg`           | 3.0     | How much your head tilts down while checking the mirror.                                                                             |
| `smoothing_time`             | 0.35    | How smooth the look-and-return motion is.                                                                                            |
| `require_stationary`         | true    | Only check the mirror when the truck is stopped.                                                                                     |
| `ignore_after_moving_signal` | true    | Don't check the mirror for a turn signal you switched on while already driving, even after you stop.                                 |
| **Manual Look**              |         |                                                                                                                                      |
| `look_angle_deg`             | 45.0    | How far your head turns when you look left or right.                                                                                 |
| `smoothing_time`             | 0.35    | How smooth the look-and-return motion is.                                                                                            |
| `toggle_mode`                | false   | On: press once to look, press again to look back. Off: look while held, let go to look back.                                         |
| **Road Irregularity**        |         |                                                                                                                                      |
| `intensity`                  | 1.0     | How strong the shake from rough road texture feels.                                                                                  |
| `reactivity`                 | 0.05    | How sharp and buzzy the road-texture shake feels. Lower = sharper.                                                                   |
| **Speed Shake**              |         |                                                                                                                                      |
| `intensity`                  | 1.0     | Overall strength of the shake you feel from driving fast.                                                                            |
| `smoothing_time`             | 0.3     | How the shake feels over time. Higher = slower and smoother, lower = jumpier and more nervous.                                       |
| `rotation`                   | 1.0     | How much your view tilts/turns as part of the shake, versus just moving side to side. Set to 0 to keep only the side-to-side motion. |
| `vertical`                   | 1.0     | How much up-and-down bounce is mixed into the shake.                                                                                 |
| `roughness`                  | 0.5     | How uneven the shake feels over time. 0 keeps it constant; higher gives bursts of calm and rough stretches.                          |
| **Body Dynamics**            |         |                                                                                                                                      |
| `lean_strength`              | 1.0     | How much your head leans toward the outside of a turn.                                                                               |
| `nod_strength`               | 1.0     | How much your head dips forward when braking and pulls back when accelerating.                                                       |
| `smoothing_time`             | 0.25    | How quickly the lean and nod settle. Lower = snappier.                                                                               |
| **Engine Start/Stop**        |         |                                                                                                                                      |
| `intensity`                  | 0.1     | How strong the shudder feels when the engine starts or stops.                                                                        |
| `duration`                   | 1.0     | How long the start-up shudder lasts. The stop shudder is shorter and gentler than this.                                              |
| **Manual Zoom**              |         |                                                                                                                                      |
| `zoom_fov_deg`               | 40.0    | How zoomed in the view gets while holding the zoom key. Lower = more zoomed in.                                                      |
| `smoothing_time`             | 0.25    | How smooth the zoom in/out feels.                                                                                                    |

Every effect also has its own `enabled` toggle, defaulting to on.

## Building

Requires [CMake][cmake] 4.4+ and [MinGW-w64][mingw-w64] (`x86_64-w64-mingw32-g++`/`windres`).

1. Rename `CMakeUserPresets.json.example` to `CMakeUserPresets.json` and fill in
   your `ETS2_PLUGINS_DIR`/`ATS_PLUGINS_DIR` paths.
   Make sure the `plugins` folder exists in your game directory.
   If it doesn’t, create it.
2. Run the workflow preset:

   ```sh
   cmake --workflow --preset user-mingw-make-release
   ```

This configures, builds, and deploys `motioncab.dll` straight into your
game's `plugins/spfPlugins/MotionCab/`.

## Project Structure

<!-- markdownlint-disable MD013 -->

```txt
├── src/
│   ├── Plugin.cpp                    Lifecycle callbacks, telemetry wiring, camera application
│   ├── Manifest.cpp / Manifest.hpp   Plugin identity, settings defaults, keybinds, UI metadata
│   ├── Links.hpp                     Plugin URLs
│   ├── PluginContext.cpp / .hpp      Shared plugin state
│   ├── math/
│   │   ├── SpringDamper.hpp          Critically-damped spring, the core smoothing primitive
│   │   └── Noise.hpp                 Deterministic gradient noise, used by Speed Shake
│   └── effects/
│       ├── Effect.hpp                Base effect interface + HeadOffset struct
│       ├── EffectManager.cpp / .hpp  Owns/drives all HeadOffset-based effects
│       ├── HeadMotionEffect.*
│       ├── BodyDynamicsEffect.*
│       ├── SteeringCameraEffect.*
│       ├── SuspensionEffect.*
│       ├── RoadIrregularityEffect.*
│       ├── SpeedShakeEffect.*
│       ├── IdleBreathingEffect.*
│       ├── EngineVibrationEffect.*
│       ├── EngineStartStopEffect.*
│       ├── MirrorCheckEffect.*
│       ├── ManualLookEffect.*
│       └── ManualZoomEffect.*
│   ├── ui/
│   │   ├── SettingsWindow.cpp / .hpp   MotionCab Quick Settings window
│   │   ├── OpenUrl.cpp / .hpp          Opens a URL in the system browser
│   │   ├── SettingsText.hpp            Shared tooltip strings (native UI + custom UI)
│   │   └── SettingsDefaults.hpp        Shared default values
│   └── ProfileManager.cpp / .hpp     Create/save/switch named presets
├── data/
│   └── logo.png                      About tab logo
├── SPF_API/                          SPF Framework SDK headers
├── cmake/
│   └── toolchain-mingw.cmake         MinGW cross-compile toolchain
├── .github/workflows/build.yaml      CI/CD
├── CMakeLists.txt                    Plugin sources, deploy steps
├── CMakePresets.json                 Toolchain/build-type presets
├── CMakeUserPresets.json             Game deploy paths
├── version.rc.in                     Windows version resource
└── LICENSE                           Project license
```

<!-- markdownlint-enable MD013 -->

## Contributing

Bug reports, feature requests, and pull requests are welcome — see
[CONTRIBUTE.md](./CONTRIBUTE.md) for build instructions, coding
standards, and how to submit a change.

## License

This project is licensed under the [GPL-3.0](./LICENSE).

[spf-framework]: https://github.com/TrackAndTruckDevs/SPF-Framework/
[spf-framework-download]: https://github.com/TrackAndTruckDevs/SPF-Framework/releases/latest
[motioncab-web]: https://motioncab.com
[cmake]: https://github.com/kitware/cmake
[mingw-w64]: https://www.mingw-w64.org/
