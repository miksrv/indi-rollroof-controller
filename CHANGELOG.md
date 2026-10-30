# Changelog

## 2.0.1

### Patch Changes

- Driver: Roll Roof now shows up in the INDI Web Manager and Ekos driver lists
- Driver: reports its real version instead of 1.0

## 2.0.0

The driver works with INDI 2.x and refuses to move the roof unless the mount is parked.

### Major Changes

- Driver: requires INDI 2.x; builds with CMake 3.18+ and installs to `/usr` by default
- Safety: the roof moves only when the mount driver reports a settled park and both reed switches are closed
- Safety: a disconnected mount driver, parking in progress or an Alert counts as "not parked"
- Driver: Roll Roof appears in the INDI Web Manager and Ekos without editing `drivers.xml`
- Driver: connecting to a board that does not answer fails after ~5 s instead of hanging
- Driver: the roof state is always read fresh, a stale limit switch reading is never reused
- Docs: setup for INDI 2, the safety interlock, pinout, controller protocol and troubleshooting

## 1.0.0

First release for INDI 1.x: roll-off roof driver over Firmata and the Arduino firmware with limit and park reed switches.
