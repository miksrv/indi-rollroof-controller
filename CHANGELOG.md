# Changelog

## 2.1.0

The roof and the telescope park switches are now shown as they are, even when the roof was moved without the driver.

### Minor Changes

- Driver: checks the roof and the park reed switches every 5 s while idle, not only around its own moves
- Driver: a roof opened or closed by other means (gate remote, power relay) updates the dome park state
- Driver: a roof stopped part-way is no longer reported as parked, so a shutdown closes it
- Driver: such a change is logged as a warning, so an unplanned roof move shows up in Ekos
- Driver: *Options → Polling* sets how often the state is checked (default 5000 ms, at least 1000)
- Driver: roof status reads `OPEN` / `CLOSED`; the log only notes changes, not every check

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
