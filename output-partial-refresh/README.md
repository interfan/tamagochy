# Partial Refresh Test Output

This folder contains a minimal nRF52840 + 1.54" e-paper test sketch for partial
refresh experiments.

Sketch:

```text
output-partial-refresh/PartialRefreshTest/PartialRefreshTest.ino
```

It draws only:

- the cat HOME screen
- status bars
- action icons
- action selection border
- cat eating animation frames

Controls:

- `LEFT`: move action selection left
- `RIGHT`: move action selection right
- `SELECT`: run cat eating animation with partial refresh, then full cleanup back to HOME
- `LEFT + RIGHT`: force full cleanup refresh

The main Tamagochi app is not changed by this test sketch.

## Build

```powershell
$env:NRF_FQBN='adafruit:nrf52:feather52840'
.\output-partial-refresh\PartialRefreshTest\build-nrf.ps1
```

## Upload

When the board is in app mode, use the app COM port, usually `COM4`:

```powershell
arduino-cli upload -p COM4 --fqbn adafruit:nrf52:feather52840 --input-dir .\output-partial-refresh\PartialRefreshTest\.nrf-build
```

When the board is in `NICENANO` bootloader mode, use the bootloader COM port,
usually `COM3`:

```powershell
arduino-cli upload -p COM3 --fqbn adafruit:nrf52:feather52840 --input-dir .\output-partial-refresh\PartialRefreshTest\.nrf-build
```

## Test Knobs

Change these constants near the top of `PartialRefreshTest.ino`:

```cpp
const byte PARTIAL_MODE = PARTIAL_ACTION_STRIP;
const bool KEEP_EPD_POWER_ON = true;
const bool HIBERNATE_AFTER_FULL_REFRESH = false;
const bool HIBERNATE_AFTER_PARTIAL_REFRESH = false;
const byte PARTIALS_BEFORE_FULL_CLEANUP = 3;
const bool EATING_ANIMATION_FULL_SCREEN_PARTIAL = false;
const unsigned long EATING_ANIMATION_FRAME_MS = 350;
```

Partial modes:

- `PARTIAL_OFF`: full refresh only, baseline.
- `PARTIAL_TIGHT_TWO_CELLS`: refresh only old and new icon cells.
- `PARTIAL_ACTION_STRIP`: refresh the full action-icon strip.
- `PARTIAL_FULL_SCREEN`: use partial waveform over the full 200x200 screen.

## Recommended Test Order

1. `PARTIAL_ACTION_STRIP`, no hibernate, keep e-paper power on.
2. Same mode, change `PARTIALS_BEFORE_FULL_CLEANUP` from `3` to `1`.
3. `PARTIAL_FULL_SCREEN`, no hibernate, keep e-paper power on.
4. `PARTIAL_TIGHT_TWO_CELLS`, no hibernate, keep e-paper power on.
5. Repeat the best result with `HIBERNATE_AFTER_PARTIAL_REFRESH = true`.
6. Repeat the best result with `KEEP_EPD_POWER_ON = false`.

Eating animation test:

1. Start with `EATING_ANIMATION_FULL_SCREEN_PARTIAL = false`.
   This refreshes the centered `184x184` animation scene window.
2. If the animation window produces artifacts, set
   `EATING_ANIMATION_FULL_SCREEN_PARTIAL = true`.
   This uses the partial waveform over the full `200x200` screen.
3. If animation ghosting appears after several runs, keep a full cleanup after
   every animation before returning to the main app.

If grey dots appear even in step 1 or 3, this panel/driver combination probably
does not have a clean partial refresh waveform with `GxEPD2_154_D67`.
