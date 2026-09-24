# Contributing to MotionCab

Thanks for taking the time to contribute!

## Building

Requires [CMake][cmake] 4.4+ and [MinGW-w64][mingw-w64]
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
   cmake --workflow --preset user-mingw-make-release
   ```

This configures, builds, and deploys `motioncab.dll` straight into your
game's `plugins/spfPlugins/MotionCab/`.

## Coding standards

- C++23.
- Every effect must be:
  - Independently toggleable and customizable.
  - Active in the interior (cabin) camera view only.
  - Wired into **both** the native SPF settings UI (`Manifest.cpp`) and
    MotionCab's own Quick Settings window (`src/ui/SettingsWindow.cpp`).
- Read the SPF SDK headers in `SPF_API/` before implementing or modifying
  any SDK interaction.
- Make sure a new feature doesn't conflict with an existing effect before
  adding it.

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
[conventional-commits]: https://www.conventionalcommits.org/
