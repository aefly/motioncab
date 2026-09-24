## Summary

<!-- What does this PR change, and why? -->

## Related issue

<!-- Closes #123, or "N/A" -->

## Type of change

- [ ] New effect
- [ ] Bug fix
- [ ] Refactor / Cleanup
- [ ] Build / CI
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
- [ ] Setting/config added to the native SPF settings UI (`Manifest.cpp`)
- [ ] Same setting mirrored in the Quick Settings window (`SettingsWindow.cpp`)
- [ ] Titles/tooltips added to `src/ui/SettingsText.hpp`
- [ ] Default value mirrored in `src/ui/SettingsDefaults.hpp`
- [ ] Checked `./SPF_API` docs before implementing/modifying an SDK interaction

## Test plan

<!--
How did you verify this works? e.g. built locally, tested in-game with
X truck/trailer, checked native UI + Quick Settings window stay in sync.
-->

- [ ] Built successfully via `cmake --workflow --preset user-mingw-make-release`
- [ ] Tested in-game
- [ ] Native settings UI and Quick Settings window both updated, if a
      setting changed

## Screenshots / Videos

<!-- For UI-visible changes (native settings UI, Quick Settings window).
     N/A otherwise. -->

## Commit style

- [ ] Commit messages follow [Conventional Commits][conventional-commits]

## Markdown

- [ ] Markdown rules follow `.markdownlint-cli2.jsonc`

[conventional-commits]: https://www.conventionalcommits.org/
