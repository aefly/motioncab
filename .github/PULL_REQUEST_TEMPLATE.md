## Summary

<!-- What does this PR change, and why? -->

## Related issue

<!-- Closes #123, or "N/A" -->

## Type of change

- [ ] New effect
- [ ] Bug fix
- [ ] Refactor / Cleanup
- [ ] Build / CI
- [ ] Localization
- [ ] Docs
- [ ] Other

## Affected effect(s)

<!-- List the effect(s) touched, or "N/A" if this isn't effect-specific. -->

## Checklist

<!-- Only tick what applies; leave unchecked items with a short reason
     if skipped. -->

- [ ] Effect is toggleable via its own "enabled" setting
- [ ] Effect is active in cabin view only
- [ ] Effect doesn't conflict with existing effects
- [ ] Effect overrides `NeedsDriverSeat()`, if it only makes sense in the
      driver's seat
- [ ] Setting declared in `settings::kAll` (`src/core/Settings.hpp`)
- [ ] New effect listed in `kEffects` (`src/ui/EffectTabs.cpp`)
- [ ] Keybind declared in `src/core/Keybinds.hpp` (`kAllActions`), if any
- [ ] Text added to every `localization/*.json` file
- [ ] `README.md`'s settings table updated, if a setting changed
- [ ] Checked `./SPF_API` docs before implementing/modifying an SDK interaction

## Test plan

<!--
How did you verify this works? e.g. built locally, tested in-game with
X truck/trailer, checked the Quick Settings window.
-->

- [ ] Built successfully via `cmake --workflow --preset user-mingw-make-release`
- [ ] Tested in-game
- [ ] Quick Settings window checked in-game, if a setting changed
- [ ] `python3 .github/scripts/check-localization.py` passes, if a
      `localization/*.json` file changed
- [ ] Changed text checked in-game in that language (reads naturally,
      fits the Quick Settings window), if a translation changed

## Screenshots / Videos

<!-- For visible changes: a video of the effect in the cabin, or a
     screenshot of the UI (Quick Settings window, native SPF windows).
     N/A otherwise. -->

## Commit style

- [ ] Commit messages follow [Conventional Commits][conventional-commits]

## Markdown

- [ ] Markdown rules follow `.markdownlint-cli2.jsonc`

[conventional-commits]: https://www.conventionalcommits.org/
