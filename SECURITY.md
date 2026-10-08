# Security Policy

## Supported versions

Only the [latest release][releases] receives security fixes. Please check
that the issue still happens on it before reporting.

## Reporting a vulnerability

Please **don't open a public issue** for a security problem. Report it
privately through GitHub instead:
[Report a vulnerability][advisory].

Include what you can of:

- the MotionCab, SPF Framework and game (ETS2/ATS) versions;
- what the problem is and what an attacker could do with it;
- the steps or files needed to reproduce it.

MotionCab is maintained in spare time, so there's no guaranteed response
time, but reports are usually acknowledged within a week. Once a fix is
released, the advisory is published with credit to you, unless you'd
rather stay anonymous.

## Scope

In scope: the MotionCab plugin (`motioncab.dll`), the files it reads
(settings, profiles, `cabin_layouts.json`, its sound bank) and the
release downloads published in this repository.

Out of scope, please report these upstream:

- SPF Framework itself: [SPF-Framework][spf];
- Euro Truck Simulator 2 and American Truck Simulator: SCS Software;
- other mods or plugins installed alongside MotionCab.

[releases]: https://github.com/aefly/motioncab/releases/latest
[advisory]: https://github.com/aefly/motioncab/security/advisories/new
[spf]: https://github.com/TrackAndTruckDevs/SPF-Framework
