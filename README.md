<p align="center">
  <img src="./.img/logo.png" alt="Logo" width="128" />
</p>

<h1 align="center">MotionCab</h1>

<p align="center"><strong>Dynamic Head-Motion for ETS2/ATS</strong></p>

<!-- markdownlint-disable MD013 -->
<p align="center">
  <a href="./LICENSE"><img alt="GitHub License" src="https://img.shields.io/github/license/aefly/motioncab?style=flat&logo=github&logoColor=white&labelColor=black&color=%23B82728" /></a>
  <a href="https://github.com/aefly/motioncab/actions"><img alt="GitHub Actions Workflow Status" src="https://img.shields.io/github/actions/workflow/status/aefly/motioncab/build.yaml?style=flat&logo=githubactions&logoColor=white&labelColor=black&color=%23B82728" /></a>
  <a href="https://github.com/aefly/motioncab/releases/latest"><img alt="GitHub Release" src="https://img.shields.io/github/v/release/aefly/motioncab?sort=semver&display_name=release&style=flat&logo=semver&logoColor=white&labelColor=black&color=%23B82728" /></a>
</p>
<!-- markdownlint-enable MD013 -->

<p align="center">
  <a href="#overview">Overview</a> •
  <a href="#features">Features</a> •
  <a href="#installation">Installation</a> •
  <a href="#game-settings">Game Settings</a> •
  <a href="#controls">Controls</a> •
  <a href="#configuration">Configuration</a>
  <br>
  <a href="#building">Building</a> •
  <a href="#project-structure">Project Structure</a> •
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
- **Suspension** — seat bounce and head tilt over bumps, plus grade follow
- **Engine Vibration** — RPM-scaled engine buzz
- **Mirror Check** — looks toward the mirror on turn signal
- **Manual Look** — smooth look-left/look-right and mirror glances
- **Road Irregularity** — ground chatter, plus rocking off-road
- **Speed Shake** — body sway growing with speed and rough ground
- **Body Dynamics** — head leans outward in corners and nods forward when
  braking
- **Engine Start/Stop** — mechanical shudder when the engine catches or
  dies
- **Natural Head Movement** — TrackIR-like head movement, without a
  tracker
- **Manual Zoom** — smooth zoom effect
- **Blindspot Viewer** — leans forward to see traffic lights hidden by the cab
- **Cabin Walk** — walks around the parked cabin and sits in the passenger
  seat or on the bunk
- **Profiles** — create, save, and switch between named presets
- **Localized** — available in 15 languages

Every effect above is independently customizable and toggleable, and only
active while in the interior (cabin) camera view.

## Installation

1. Install the [SPF Framework][spf-framework-download] in your game's
   `bin/win_x64/plugins/` folder.
2. Install [MotionCab][motioncab-web] in your game's
   `bin/win_x64/plugins/spfPlugins/` folder.
3. Start your game and enable **MotionCab** in SPF's plugin manager.

## Game Settings

For both **ETS2** and **ATS**, disable the following camera options
in `Options > Gameplay`:

| Setting                                | Value   |
| -------------------------------------- | ------- |
| Uneven surfaces simulation             | **0%**  |
| Steering camera rotation factor        | **0%**  |
| Steering camera rotation on reverse    | **Off** |
| Physical camera movement               | **Off** |
| Physical camera factor                 | **0%**  |
| Interior camera horizon locking factor | **Off** |

Disabling these settings ensures that MotionCab has full control over the camera.

## Controls

| Action                 | Default key | Behavior                                                                              |
| ---------------------- | ----------- | ------------------------------------------------------------------------------------- |
| Toggle Settings Window | F9          | Shows/hides the MotionCab Quick Settings window                                       |
| Look Left              | Numpad /    | Manual Look — hold or toggle to look left                                             |
| Look Right             | Numpad *    | Manual Look — hold or toggle to look right                                            |
| Glance at Left Mirror  | Numpad 7    | Manual Look — hold or toggle to glance at the left mirror                             |
| Glance at Right Mirror | Numpad 9    | Manual Look — hold or toggle to glance at the right mirror                            |
| Zoom                   | Numpad -    | Manual Zoom — hold to zoom in, release to return to normal                            |
| Blindspot Viewer       | F10         | Blindspot Viewer — Toggle or hold to lean forward and look up                         |
| Stand Up / Sit Down    | Page Up     | Cabin Walk — get up; tap to sit at the seat you look at, hold to go back to the wheel |
| Walk                   | W/S/A/D     | Cabin Walk — walk forward, back and sideways while standing                           |
| Crouch                 | C           | Cabin Walk — crouch while standing                                                    |

## Configuration

All parameters are tuned live in MotionCab's own **Quick Settings** window:
right-click any slider to reset it to default, and the **Settings** tab has a
button to reset every effect at once.
Values are persisted by the framework to `plugins/spfPlugins/MotionCab/config/settings.json`.

| Setting                      | Default | Description                                                                                                                          |
| ---------------------------- | ------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| **Head Motion**              |         |                                                                                                                                      |
| `sway_strength`              | 0.2     | How much your head gets pushed around when you speed up, brake, or take a turn.                                                      |
| `tilt_strength`              | 0.15    | How much your head tilts when the cab leans or rocks.                                                                                |
| `smoothing_time`             | 0.2     | How quickly your head settles into place. Higher = slower and smoother, lower = snappier.                                            |
| **Steering Camera**          |         |                                                                                                                                      |
| `rotation_left_deg`          | 30.0    | How far the camera turns when you crank the wheel all the way to the left.                                                           |
| `rotation_right_deg`         | 30.0    | How far the camera turns when you crank the wheel all the way to the right.                                                          |
| `center_zone_pct`            | 15.0    | How much of the wheel's turn keeps the camera turning alike both ways. Past it, each side turns at its own rotation amount.          |
| `smoothing_time`             | 0.5     | How smooth the camera turn feels once it gets going. Higher = smoother and slower.                                                   |
| `delay_seconds`              | 0.2     | How long the camera waits after you turn the wheel before it starts moving.                                                          |
| `disable_in_reverse`         | true    | Keep the camera centered while the truck is in reverse gear.                                                                         |
| **Idle Breathing**           |         |                                                                                                                                      |
| `vertical_amplitude`         | 0.002   | How far the camera rises and falls with each breath.                                                                                 |
| `pitch_amplitude_deg`        | 0.15    | How much your head tips forward/back with each breath.                                                                               |
| `breathing_rate_bpm`         | 15.0    | How fast the breathing happens.                                                                                                      |
| `fade_start_kmh`             | 30.0    | The speed at which this effect starts fading away as you speed up.                                                                   |
| `fade_end_kmh`               | 50.0    | The speed at which this effect has completely faded away.                                                                            |
| **Suspension**               |         |                                                                                                                                      |
| `vertical_strength`          | 1.0     | How much bumps in the road bounce and tilt the camera.                                                                               |
| `reactivity`                 | 0.25    | How quickly the seat settles after a bump. Lower = firmer and snappier.                                                              |
| `grade_strength`             | 0.5     | Raises your head on downhill stretches and lowers it on uphill ones.                                                                 |
| **Engine Vibration**         |         |                                                                                                                                      |
| `intensity`                  | 1.0     | How strong the engine rumble feels.                                                                                                  |
| **Mirror Check**             |         |                                                                                                                                      |
| `look_angle_deg`             | 28.0    | How far your head turns to check the mirror.                                                                                         |
| `pitch_offset_deg`           | 3.0     | How much your head tilts down while checking the mirror.                                                                             |
| `smoothing_time`             | 0.35    | How smooth the look-and-return motion is.                                                                                            |
| `require_stationary`         | true    | Only check the mirror when the truck is stopped.                                                                                     |
| `ignore_after_moving_signal` | true    | Don't check the mirror for a turn signal you switched on while already driving, even after you stop.                                 |
| **Manual Look**              |         |                                                                                                                                      |
| `look_left_deg`              | 45.0    | How far your head turns when you look left.                                                                                          |
| `look_right_deg`             | 45.0    | How far your head turns when you look right.                                                                                         |
| `glance_left_deg`            | 20.0    | How far your head turns when you glance at the left mirror.                                                                          |
| `glance_left_pitch_deg`      | 3.0     | How much your head tilts down while glancing at the left mirror.                                                                     |
| `glance_right_deg`           | 30.0    | How far your head turns when you glance at the right mirror.                                                                         |
| `glance_right_pitch_deg`     | 3.0     | How much your head tilts down while glancing at the right mirror.                                                                    |
| `smoothing_time`             | 0.35    | How smooth the look-and-return motion is.                                                                                            |
| `toggle_mode`                | false   | On: press once to look, press again to look back. Off: look while held, let go to look back.                                         |
| **Road Irregularity**        |         |                                                                                                                                      |
| `intensity`                  | 1.0     | How strong the road-texture shake and off-road rocking feel.                                                                         |
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
| **Natural Head Movement**    |         |                                                                                                                                      |
| `tremor_intensity`           | 5.0     | How much your head trembles slightly, all the time.                                                                                  |
| `micro_intensity`            | 2.5     | How far your head moves when it makes a small adjustment.                                                                            |
| `posture_intensity`          | 1.3     | How far your head shifts when it settles into a new position: tilted, leaning forward or to one side.                                |
| `posture_interval`           | 15.0    | About how long your head holds a position before settling into a new one.                                                            |
| `steering_tilt`              | 1.5     | How much your head tilts into the turn as you turn the steering wheel.                                                               |
| **Manual Zoom**              |         |                                                                                                                                      |
| `zoom_fov_deg`               | 40.0    | How zoomed in the view gets while holding the zoom key. Lower = more zoomed in.                                                      |
| `smoothing_time`             | 0.25    | How smooth the zoom in/out feels.                                                                                                    |
| **Blindspot Viewer**         |         |                                                                                                                                      |
| `pos_x`                      | -0.06   | How far your head moves left or right while peeking.                                                                                 |
| `pos_y`                      | -0.1    | How far your head moves up or down while peeking.                                                                                    |
| `pos_z`                      | -0.88   | How far your head moves forward while peeking. Negative = forward.                                                                   |
| `yaw_deg`                    | -1.7    | How far your head turns left or right while peeking.                                                                                 |
| `pitch_deg`                  | 33.0    | How far your head tilts up or down while peeking.                                                                                    |
| `roll_deg`                   | 3.0     | How far your head tilts to the side while peeking.                                                                                   |
| `fov_offset_deg`             | 10.0    | How much the field of view widens while peeking. Negative = narrower.                                                                |
| `smoothing_time`             | 0.6     | How smooth the lean-and-return motion is.                                                                                            |
| `toggle_mode`                | true    | On: press once to peek, press again to sit back. Off: peek while held, let go to sit back.                                           |
| **Cabin Walk**               |         |                                                                                                                                      |
| `require_parking_brake`      | true    | On: needs the parking brake, releasing it sends you back. Off: stopping is enough.                                                   |

Every effect also has its own `enabled` toggle, defaulting to on.

## Building

Requires [CMake][cmake] 4.4+, [Ninja][ninja] and [MinGW-w64][mingw-w64] (`x86_64-w64-mingw32-g++`/`windres`).

1. Rename `CMakeUserPresets.json.example` to `CMakeUserPresets.json` and fill in
   your `ETS2_PLUGINS_DIR`/`ATS_PLUGINS_DIR` paths.
   Make sure the `plugins` folder exists in your game directory.
   If it doesn’t, create it.
2. Run the workflow preset:

   ```sh
   cmake --workflow --preset user-mingw-release
   ```

This configures, builds, and deploys `motioncab.dll` straight into your
game's `plugins/spfPlugins/MotionCab/`.

## Project Structure

<!-- markdownlint-disable MD013 -->

```txt
├── src/
│   ├── Plugin.cpp                    Lifecycle callbacks, telemetry wiring, per-frame driver
│   ├── core/
│   │   ├── Manifest.cpp / .hpp       Plugin identity, settings defaults, keybinds, window
│   │   ├── CameraRig.cpp / .hpp      Writes the effects' offset into the interior camera
│   │   ├── PluginContext.cpp / .hpp  Shared plugin state
│   │   ├── Profiles.cpp / .hpp       Create/save/switch named presets
│   │   ├── Settings.cpp / .hpp       Every setting: key, default, slider range
│   │   ├── Conflicts.hpp             Which effects pause which while enabled
│   │   ├── Loc.cpp / .hpp            Translated string lookup (loc::Tr)
│   │   ├── Keybinds.hpp              Keybind action names and key polling helpers
│   │   ├── Strings.hpp               Case-insensitive string helpers
│   │   └── Links.hpp                 Plugin URLs
│   ├── math/
│   │   ├── SpringDamper.hpp          Critically-damped spring, the core smoothing primitive
│   │   ├── Noise.hpp                 Deterministic gradient noise
│   │   ├── Oscillator.hpp            Damped spring that can overshoot and ring
│   │   └── Units.hpp                 Unit conversions, easing, Lerp, phase wrapping
│   ├── effects/
│   │   ├── Effect.hpp                Base effect interface + HeadOffset struct
│   │   ├── ConfigurableEffect.hpp    Base for effects: enabled toggle + settings access
│   │   ├── EffectManager.cpp / .hpp  Owns/drives all HeadOffset-based effects
│   │   ├── Telemetry.hpp             Telemetry readings shared by effects
│   │   ├── driving/                  HeadMotion, BodyDynamics, SteeringCamera
│   │   ├── road/                     Suspension, RoadIrregularity, SpeedShake,
│   │   │                             Surface (ground classification)
│   │   ├── cabin/                    IdleBreathing, EngineVibration, EngineStartStop,
│   │   │                             NaturalHeadMovement
│   │   └── manual/                   MirrorCheck, ManualLook, ManualZoom, BlindspotViewer
│   │       └── cabin_walk/
│   │           ├── CabinWalkEffect.*     The effect: standing up, walking, sitting down
│   │           ├── CabinLayouts.*        Per-truck cabin layouts
│   │           ├── CabinPresets.*        Shipped layouts, one per truck model
│   │           ├── CabinWalkSounds.*     Footsteps (FMOD bank)
│   │           └── InteriorCameraOverride.* Swaps the camera's limits while out of the seat
│   └── ui/
│       ├── SettingsWindow.cpp / .hpp MotionCab Quick Settings window
│       ├── EffectTabs.cpp / .hpp     Driving/Road/Cabin/Manual tabs
│       ├── CabinLayoutPanel.cpp/.hpp Cabin Walk's layout panel (Advanced)
│       ├── SettingsTab.cpp / .hpp    Settings tab: keybind, profiles, reset
│       ├── AboutTab.cpp / .hpp       About tab
│       ├── Widgets.cpp / .hpp        Shared look and widgets
│       └── OpenUrl.cpp / .hpp        Opens a URL in the system browser
├── data/
│   ├── logo.png                      About tab logo
│   └── sounds/                       Cabin Walk's FMOD bank
├── localization/
│   └── <lang>.json                   UI text, one file per language
├── SPF_API/                          SPF Framework SDK headers
├── cmake/
│   └── toolchain-mingw.cmake         MinGW cross-compile toolchain
├── .github/workflows/build.yaml      CI/CD
├── .github/scripts/
│   └── check-localization.py         Checks every language file against en.json
├── CMakeLists.txt                    Plugin sources, deploy steps
├── CMakePresets.json                 Toolchain/build-type presets
├── CMakeUserPresets.json.example     Template for your game deploy paths
├── version.rc.in                     Windows version resource
└── LICENSE                           Project license
```

<!-- markdownlint-enable MD013 -->

## Contributing

Bug reports, feature requests, pull requests and translation fixes are
welcome — see [CONTRIBUTING.md](./CONTRIBUTING.md) for build instructions,
coding standards, how to improve a translation or add a language, and
how to submit a change.

## License

This project is licensed under the [GPL-3.0](./LICENSE).

[spf-framework]: https://github.com/TrackAndTruckDevs/SPF-Framework/
[spf-framework-download]: https://github.com/TrackAndTruckDevs/SPF-Framework/releases/latest
[motioncab-web]: https://motioncab.com
[cmake]: https://github.com/kitware/cmake
[mingw-w64]: https://www.mingw-w64.org/
[ninja]: https://ninja-build.org/
