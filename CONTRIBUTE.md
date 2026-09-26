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
`src/Localization.hpp`). A setting's title and description live under
its own config key plus `.title`/`.desc`
(e.g. `settings.road.suspension.reactivity.title`).

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
