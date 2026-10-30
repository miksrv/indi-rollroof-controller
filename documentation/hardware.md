# Roof Controller Hardware

Reference for the Arduino roof controller (`arduino-firmware/RoofController`) and how the INDI driver talks to it.

## Pinout

| Pin | Sketch name | Role | Mode | Active level |
|-----|-------------|------|------|--------------|
| D2 | `pinRelayOpen` | Relay, pulsed by `actionRoofClose()` | OUTPUT, idle HIGH | LOW |
| D3 | `pinRelayClose` | Relay, pulsed by `actionRoofOpen()` | OUTPUT, idle HIGH | LOW |
| D4 | `pinLimitSwitchClose` | Roof fully **closed** limit switch | INPUT_PULLUP | LOW = pressed |
| D5 | `pinLimitSwitchOpen` | Roof fully **open** limit switch | INPUT_PULLUP | LOW = pressed |
| D6 | `parkTelescopeDEC` | Telescope park reed switch, DEC axis | INPUT_PULLUP | LOW = closed (DEC at park) |
| D7 | `parkTelescopeRA` | Telescope park reed switch, RA axis | INPUT_PULLUP | LOW = closed (RA at park) |
| D13 | `ledPin` | Debug LED (only with `#define DEBUG`) | OUTPUT | — |

Notes:
- The relay **names** in the sketch are swapped relative to what they do: the OPEN command pulses D3 (`pinRelayClose`) and CLOSE pulses D2 (`pinRelayOpen`). Wire the relays by function (the D3 relay must start opening the roof), not by the variable name.
- Switches and reed switches connect the pin to GND. With `INPUT_PULLUP` an unconnected or broken wire reads HIGH, i.e. "not pressed" / "not parked", which is the safe state.

## Roof drive

The roof is driven by a sliding-gate motor controller. Each command is a single 800 ms relay pulse; the gate controller then runs the full travel on its own (about 10 s on the reference installation) and stops at its own end positions.

- A running travel cannot be stopped from the Arduino, so `ABORT` is a no-op in the firmware. The driver's ABORT only stops waiting for the limit switch.
- The driver waits at most 19 s for the target limit switch. If the travel takes longer on your installation, increase `MAX_ROLLOFF_DURATION` in `rollroof.cpp`.

## Protocol

The driver talks Firmata at 57600 baud and sends commands as Firmata `STRING_DATA`:

| Command | Effect |
|---------|--------|
| `OPEN` | Pulses the opening relay — only if both park reed switches are closed |
| `CLOSE` | Pulses the closing relay — only if both park reed switches are closed |
| `ABORT` | No action (see above) |
| `QUERY` | Answers with a two-digit status string, no relay action |

`QUERY` answer = roof digit followed by park digit:

| Roof digit | Meaning |
|------------|---------|
| `1` | Open: D5 LOW and D4 HIGH |
| `2` | Closed: D4 LOW and D5 HIGH |
| `3` | Unknown: both or neither pressed (normal while the roof is moving) |

| Park digit | Meaning |
|------------|---------|
| `0` | Parked: D6 and D7 LOW |
| `1` | Not parked on either axis |
| `2` | DEC not parked |
| `3` | RA not parked |

Examples: `20` — roof closed, telescope parked; `10` — roof open, telescope parked; `30` — roof moving, telescope parked; `13` — roof open, RA not at park.

The comment above the `QUERY` handler in the sketch has the open/closed codes the other way round; the code (and this table) is what the driver relies on.

## Troubleshooting

- **The driver times out while opening, the roof is open.** The open limit switch (D5) was never seen. Check the switch, its wire, and that the roof actually reaches it — a lost mechanical stop lets the roof travel past the switch.
- **The roof reads "closed" while it is open.** D4 is LOW: check the closed switch and its wiring, and that the D4 line is not shared with the gate controller's own end-stop wiring.
- **"NOT PARKED by the reed switches" with the mount at park.** Check the magnets and the D6/D7 wiring, and that the mount driver's park position matches the reed switches.
- **"NOT PARKED in software"** — the mount driver set as `ACTIVE_TELESCOPE` is not running, not connected, not parked, or its park state is Busy/Alert. Reconnecting the mount driver clears a stale Alert.
- **Connection fails.** The board answers the Firmata version request within ~5 s after the port opens (the board resets on open); check the port and that nothing else (e.g. the mount driver's auto search) holds it.
