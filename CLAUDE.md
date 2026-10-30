# CLAUDE.md

**indi-rollroof-controller** — an INDI 2.x dome driver (`indi_rollroof`, device **Roll Roof**) and Arduino firmware for a roll-off observatory roof driven by a sliding-gate motor. The driver talks to the Arduino over Firmata; the roof is opened/closed by relay pulses and its state is read from limit switches and telescope park reed switches.

## Layout

- `indi-driver/indi-rollroof/` — the driver: `rollroof.cpp/.h` (`INDI::Dome` subclass), `indi_rollroof.xml.cmake` (template of the driver description installed to `/usr/share/indi`; keep the `<driversList>` root, the INDI Web Manager and KStars skip files without it), `CMakeLists.txt`
- `indi-driver/indi-rollroof/libfirmata/` — bundled Firmata client (`firmata.cpp`, `arduino.cpp` serial port)
- `indi-driver/build.sh` — configure, build and `sudo make install`
- `arduino-firmware/RoofController/RoofController.ino` — firmware (StandardFirmata-based, 57600 baud)
- `documentation/hardware.md` — pinout, signal levels, protocol (`OPEN`/`CLOSE`/`ABORT`/`QUERY` and the QUERY codes), troubleshooting
- `models/` — 3D-printable case

## Commands

```bash
# Driver (needs INDI 2.x dev files: libindi-dev from ppa:mutlaqja/ppa, plus libnova-dev)
./indi-driver/build.sh                                   # build + sudo make install
cmake -B build indi-driver/indi-rollroof && cmake --build build   # build only

# Firmware compile check (as in CI)
arduino-cli core install arduino:avr && arduino-cli lib install Firmata
arduino-cli compile --fqbn arduino:avr:uno arduino-firmware/RoofController
```

There are no unit tests. CI (`.github/workflows/`): `driver-build.yml` builds and installs the driver in an `ubuntu:26.04` container with INDI 2.x from the INDI PPA (older Ubuntu releases only have INDI 1.x; the `ubuntu-26.04` runner itself prefers amd64v3 archive packages, i.e. INDI 1.9); `firmware-build.yml` compiles the sketch with `arduino/compile-sketches`. Actions are pinned by commit SHA with the version in a comment; Dependabot (`.github/dependabot.yml`) updates them. Real verification happens on the observatory host with the hardware.

## Safety — read before changing the driver or firmware

At park the telescope tube lies just under the roof; in any other position the moving roof hits it, and the gate motor is strong enough to wreck the equipment.

- The roof may move **only** when the mount driver (`ACTIVE_TELESCOPE`) reports `TELESCOPE_PARK` = PARK with state Ok **and** both park reed switches are closed. Unknown, disconnected, Busy and Alert count as not parked. Never relax, bypass or reorder these checks in `RollRoof::Move()`, and never add a code path that sends `OPEN`/`CLOSE` without going through it.
- The firmware independently ignores `OPEN`/`CLOSE` unless both reed switches are closed. Keep that check.
- A travel cannot be stopped once started (gate controller); `ABORT` only stops the driver waiting. Do not document or rely on it as an emergency stop.
- The driver only refuses; it never parks the mount. Automatic shutdown order is mount first, roof second (Ekos Scheduler / WatchDog).
- Testing on real hardware: never send a motion command just to read the state (use `QUERY` or reconnect), and only move the roof with the owner watching and explicitly agreeing to that motion.

## Conventions

- C++17 against the INDI 2 API: `LOG_*`/`LOGF_*` macros, `INDI::PropertySwitch` accessors (`DomeMotionSP[...]`, `getState()`), `defineProperty`. No global `IS*` entry points — INDI dispatches to the device instance.
- Match the surrounding style of the file you edit (it mixes tabs and spaces from the upstream code; don't reformat untouched lines).
- When the pinout, the protocol or the QUERY codes change, update `documentation/hardware.md`; when the interlock or the driver setup changes, update the README ("Configuring the Driver", "Safety Interlock").
- The firmware name must keep containing `RoofController` (the driver checks the Firmata firmware name on connect).
- Documentation and commit messages in English.

## Versioning and CHANGELOG

The version lives in one place: `VERSION_MAJOR`/`VERSION_MINOR`/`VERSION_PATCH` in `indi-driver/indi-rollroof/CMakeLists.txt`. CMake generates the driver XML from `indi_rollroof.xml.cmake` (`<version>X.Y.Z</version>`, shown in the INDI Web Manager / Ekos) and passes major/minor to the driver for `DRIVER_INFO` (INDI keeps only `X.Y` there). Releases are `X.Y.Z`.

Before opening a PR that should be released:

1. Bump the version: **patch** for fixes, **minor** for new features or behaviour changes, **major** for breaking changes (new INDI major, changed protocol that needs a firmware reflash).
2. Add a section for that version to the top of `CHANGELOG.md` in the existing format: `## X.Y.Z`, a one-sentence summary for a release with several changes, then `### Patch Changes`, `### Minor Changes` or `### Major Changes`, then short one-line bullets (under ~100 characters), e.g. `- Driver: connecting to a board that does not answer fails after ~5 s instead of hanging`. Written for observatory owners: what they notice when installing or running the roof, the most important first, no internals; small fixes of the same release are not listed.

Branches for a release are named `release/X.Y.Z`; other work for that release is merged into it, not into `main`.

## Release

Only after the PR is merged into `main`: tag the merge commit `v<version>` and create the GitHub release (title = tag, body = that version's `CHANGELOG.md` section without the `## X.Y.Z` heading, marked latest) in one command:
`gh release create v2.0.0 --target main --title v2.0.0 --notes-file <section.md> --latest`.
