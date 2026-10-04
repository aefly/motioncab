# Contributing to MotionCab

Thanks for taking the time to contribute!

## Building

Requires [CMake][cmake] 4.4+, [Ninja][ninja] and [MinGW-w64][mingw-w64]
(`x86_64-w64-mingw32-g++`/`windres`).

1. Clone or fork the repository:

   ```sh
   git clone https://github.com/aefly/motioncab.git
   cd motioncab
   ```

2. Rename `CMakeUserPresets.json.example` to `CMakeUserPresets.json`
   and fill in your `ETS2_PLUGINS_DIR`/`ATS_PLUGINS_DIR` paths.
   Make sure the `plugins` folder exists in your game directory.
   If it doesn’t, create it.
3. Run the workflow preset:

   ```sh
   cmake --workflow --preset user-mingw-release
   ```

This configures, builds, and deploys `motioncab.dll` straight into your
game's `plugins/spfPlugins/MotionCab/`.

## Coding standards

- C++23.
- Every effect must be:
  - Independently toggleable and customizable.
  - Active in the interior (cabin) camera view only.
  - Shown in MotionCab's own Quick Settings window. Declare each setting
    once in `settings::kAll` (`src/core/Settings.hpp`): the
    manifest, the Quick Settings window and the profiles are all generated
    from it. A new effect also needs its entry in `kEffects`
    (`src/ui/EffectTabs.cpp`).
- Read the SPF SDK headers in `SPF_API/` before implementing or modifying
  any SDK interaction.
- Make sure a new feature doesn't conflict with an existing effect before
  adding it.
- Derive a new effect from `ConfigurableEffect`
  (`src/effects/ConfigurableEffect.hpp`), which handles its `enabled`
  toggle and reads its settings by name.
- If an effect only makes sense in the driver's seat, override
  `NeedsDriverSeat()` so it fades out while Cabin Walk has the player
  elsewhere in the cabin.
- Declare a new keybind action once in `src/core/Keybinds.hpp` (default
  key and localization key) and add it to `kAllActions`, plus
  `kPolledActions` if the effect polls it.
- Reuse the shared helpers (`src/math/Units.hpp`,
  `src/effects/Telemetry.hpp`, `src/effects/road/Surface.hpp`,
  `src/core/Keybinds.hpp`) rather than duplicating their logic in an
  effect.

## Code layout

- `src/Plugin.cpp` is the entry point and stays alone at the `src/` root.
  Shared code goes in `src/core/`, the math primitives in `src/math/`,
  the Quick Settings window in `src/ui/`, and each effect in
  `src/effects/<group>/`, the group being its `settings.<group>.*` one
  (`driving`, `road`, `cabin` or `manual`).
- Includes: a `.cpp` includes its own header first, by its bare name
  (`"Foo.hpp"`). Every other project header goes by its path from `src/`
  (`"core/PluginContext.hpp"`), even from the same folder. Keep each block
  of `#include "..."` lines sorted.
- Namespaces: classes and structs live in `motioncab`. A module of free
  functions or constants gets its own `motioncab::<module>`, named after
  its file (`Profiles.*` is `profiles`, `Loc.*` is `loc`). Everything in
  `src/ui/` is in `motioncab::ui`, everything in `src/math/` in
  `motioncab::math`. Write the nested form (`namespace motioncab::ui {`)
  and close it with `} // namespace motioncab::ui`.
- Sources aren't globbed: add a new `.cpp` to `PLUGIN_SOURCES` in
  `CMakeLists.txt`, under its folder's group, in alphabetical order
  (`src/Plugin.cpp` stays first).

## Localization

MotionCab's text lives in `localization/<language code>.json`, one file
per language. `en.json` is the reference: every other file must have
exactly the same keys. Only the English text is original; the other
languages haven't been reviewed by native speakers yet, so some strings
may be clumsy or plain wrong. Corrections are welcome.

### Improving a translation

1. Open `localization/<code>.json` and change the text on the right-hand
   side only.
2. Keep these as they are in `en.json`:
   - placeholders such as `{name}`, `{version}` or `{settings_tab}`, which
     the plugin fills in at runtime;
   - `**bold**` markers, and file names such as `MotionCab.log`.
3. Check your file:

   ```sh
   python3 .github/scripts/check-localization.py
   ```

4. Build, pick your language in the plugin's language setting in SPF's
   settings window, and check in-game that the text reads naturally and
   fits (the Quick Settings window's labels have limited room).

If you'd rather not touch the files, open an issue with the wrong text,
where it shows up, and your suggested wording.

### Adding a language

1. Copy `localization/en.json` to `localization/<code>.json`. Use the
   same code as SPF Framework's own language files (e.g. `pt_br`, `zh`),
   so SPF's "sync plugin languages" option can match it.
2. Translate every value. Don't leave English text behind, as SPF has no
   per-key fallback: a missing key shows up raw in-game.
3. In the `language` section, add your language's display name to
   **every** file, in that file's language (e.g. `"de": "German"` in
   `en.json`, `"de": "Deutsch"` in `de.json`). This is what the language
   dropdown shows.
4. Run the check script above and test in-game as for any translation.

No code change is needed: SPF discovers the files on its own, and the
build copies the whole `localization/` folder next to the DLL. SPF's
font covers Latin, Cyrillic, Chinese, Japanese and Korean; other scripts
may not render.

### Adding text in code

Never hardcode user-facing text. Add a key to `en.json` and every other
language file, then look it up with `loc::Tr()` (see
`src/core/Loc.hpp`). A setting's title and description live under
its own config key plus `.title`/`.desc`
(e.g. `settings.road.suspension.reactivity.title`).
`check-localization.py` also fails if `en.json` lacks a key the code needs
for a setting, a settings group or effect, an effect's hint or a keybind.

## Commit messages

Commits must follow [Conventional Commits][conventional-commits]
(`feat:`, `fix:`, `refactor:`, `docs:`, etc.).

## Markdown

Any Markdown file you add or change must follow the rules in
`.markdownlint-cli2.jsonc`.

## Submitting a change

1. Fork the repository and create a branch for your change.
2. Build and test your change in-game.
3. Open a pull request using the template — fill in the test plan
   honestly.

## Reporting bugs / requesting features

Please use the issue templates (Bug Report / Feature Request) rather than
a blank issue — they ask for the details needed to reproduce or evaluate
the request.

[cmake]: https://github.com/kitware/cmake
[mingw-w64]: https://www.mingw-w64.org/
[ninja]: https://ninja-build.org/
[conventional-commits]: https://www.conventionalcommits.org/
