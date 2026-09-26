# TamagochiNrfEink

Standalone physical-device build for a nice!nano / Pro Micro-compatible
nRF52840 board plus a 1.54-inch black/white e-paper display. This folder is
not for Wokwi.

The attached board reports as:

```text
Board-ID: nRF52840-nicenano
```

The back of the PCB is marked:

```text
promicro v1940
```

## Files

- `TamagochiNrfEink.ino` - hardware sketch
- `companion_bitmaps.h`
- `animal_idle_variants.h`
- `action_icons.h`
- `status_bitmaps.h`
- `species_action_bitmaps.h`
- `build-nrf.ps1` - Arduino CLI wrapper; generates a new firmware build ID
- `build_stamp.h` - generated build ID used to decide whether a save can be loaded

## Gameplay and Controls

This sketch shares the nine animals, care actions, three mini-games,
English/Bulgarian/German text, and 25-day grown-up ending described in the
[project README](../../README.md). The Play menu also includes Info, which
shows age and weight calculated from species, age, and current condition.

- Left/Right select an action or menu entry; Select confirms.
- Three Select presses on the egg screen, at most 700 ms apart, hatch it early.
- Mute normally toggles sound. In hospital it instead counts toward a
  12-press shortcut that immediately recovers the pet.
- Holding Select for 5 seconds wakes the pet from Overnight or exhaustion sleep.
  The same control works after restoring a sleeping pet on restart.
- The root sketch's HOME debug/hospital/grown-up button sequences are absent.

Settings code exists for changing the clock or starting a new egg, but the
HOME action strip and selector exclude Settings, making that menu unreachable
through normal controls. Learning points are tracked but do not unlock anything.

## Pet Sleep and Hospital Protection

The Overnight action sets energy to 100, lowers food to at most 20, and puts
the pet to sleep **until the user holds Select for 5 seconds**. Exhaustion
uses the same sleep scene and wake control. There is no automatic wake after
12 hours or any other duration. Short Select presses and Left/Right do not
perform care actions during this sleep; Mute still works. An ordinary Nap
remains a separate sleep toggle.

Deep sleep is saved and restored, including the sleep scene and manual wake
control. Age continues advancing while asleep, but the grown-up ending waits
until after the user wakes the pet.

`hospitalBlockedByDeepSleep()` prevents hospital admission while
`deepSleepAwaitingWake` is set or the post-sleep grace counter is nonzero.
On manual wake, `wakeFromDeepSleep()` starts **60 game minutes of hospital
protection**. This counter decreases once per game minute and is included in
saves. The hour only starts when the user wakes the pet.

Needs and health continue changing during sleep and the grace period; waking
does not heal the pet. Feed, water, and restore health with Medicine or Bath
as needed during this hour. After the protection expires, the next needs
update that finds health at 0 can send the pet to hospital. A Nap by itself
does not grant the grace period. The root/Wokwi sketch lacks this protection.

Hospital lasts 24 hours. Its internal timer decreases every minute, while the
display shows remaining hours rounded up and changes once per hour. Recovery
restores minimum care stats and clears dirt, poop, sickness, and viruses.

## Required Libraries

Install these in Arduino IDE / Arduino CLI:

- `Adafruit GFX Library`
- `GxEPD2`
- `U8g2_for_Adafruit_GFX`
- An nRF52840 Arduino core

On Adafruit nRF52, saves are stored in internal LittleFS as
`/tamagochi.sav`.

## Default Pins

Buttons connect from pin to `GND`; the sketch uses `INPUT_PULLUP`.

The sketch is compiled with `adafruit:nrf52:feather52840` because the installed
Adafruit core does not include a native nice!nano board. Therefore the code
uses Feather Arduino pin numbers that map to the raw nRF pins on the
Pro Micro-labeled board.

Wire by the `Pro Micro pad` column.

| Signal | Pro Micro pad | Raw nRF pin | Code pin |
| --- | --- | --- | --- |
| Left button | D0 / TX | P0.06 | `11` |
| Select button | D1 / RX | P0.08 | `12` |
| Right button | D10 / NFC1 | P0.09 | `33` |
| Mute button | D11 / NFC2 | P0.10 | `2` |
| Buzzer | D7 / SCL | P0.11 | `23` |
| E-paper CS | D5 / CS | P0.24 | `1` |
| E-paper DC | D15 / A0 | P0.02 | `18` |
| E-paper RST | D16 / A1 | P0.29 | `20` |
| E-paper BUSY | D17 / A2 | P0.31 | `21` |
| E-paper MOSI / DIN | D4 / MOSI | P0.22 | `30` |
| E-paper SCK / CLK | D2 / SCK | P0.17 | `29` |
| E-paper MISO | not connected | P0.20 dummy | `28` |
| E-paper VCC | 3V3 | 3.3V rail | n/a |
| E-paper GND | GND | Ground | n/a |

The e-paper does not need MISO. It is only configured because the Adafruit nRF
SPI API asks for one.

If your board labels pins differently, edit the `#define *_PIN` section near
the top of `TamagochiNrfEink.ino`.

## Build

Set your board FQBN if needed:

```powershell
$env:NRF_FQBN="adafruit:nrf52:feather52840"
.\build-nrf.ps1
```

The default FQBN in the script is:

```text
adafruit:nrf52:feather52840
```

Use the exact FQBN for your installed nRF52840 core/board package.

If you use the Adafruit nRF52 core, install the board package first:

```powershell
arduino-cli config add board_manager.additional_urls https://adafruit.github.io/arduino-board-index/package_adafruit_index.json
arduino-cli core update-index
arduino-cli core install adafruit:nrf52
```

The Adafruit core does not include a native nice!nano board entry, so the
current build target is:

```text
adafruit:nrf52:feather52840
```

Compile:

```powershell
$env:NRF_FQBN="adafruit:nrf52:feather52840"
.\build-nrf.ps1
```

Upload through serial DFU while the board is in bootloader mode:

```powershell
arduino-cli upload -p COM3 --fqbn adafruit:nrf52:feather52840 --input-dir .\.nrf-build
```

Plain UF2 copy did not flash this bootloader reliably. Serial DFU reported
`Device programmed`.

## MCU Sleep and Display Behavior

This build uses real nRF System ON sleep:

- E-paper stays awake during the 60-second active window so partial refresh can use a known previous frame.
- The V1940/nice!nano-compatible VCC switch on raw `P0.13` is held HIGH while peripherals are powered and driven LOW when entering MCU sleep after display hibernation.
- Programmable indicator candidates `P0.15`, `P0.16`, `P1.10`, and `P1.15` are forced HIGH/off.
- A refresh with no known previous frame uses a full update, including boot and the next redraw after MCU sleep. Screen changes generally use full refresh; transitions between initial setup screens can use partial refresh.
- Partial refresh is used for supported same-screen changes, including egg wobble, hatching frames, HOME, menus, the hospital timer, and action animation frames.
- HOME action selection can refresh only the bottom strip; meter changes can refresh only the top status window.
- General partial refreshes have a limit of 3 before a full cleanup. HOME and initial setup updates, and the dedicated HOME action-strip/status paths, bypass that limit while the previous frame remains valid.
- After 60 seconds without input, the MCU sleeps.
- RTC2 wakes the MCU every 60 seconds to update timers/stats.
- Button GPIO wake exits sleep immediately.
- Hospital display changes once per hour; its internal countdown still advances every minute.
- HOME idle animation refreshes every 10 seconds during the active window and stops outside it.
- Low-status flashing is disabled. Audible status alerts remain enabled when unmuted.

If a red/orange charge LED remains on while USB is connected, that is charger
hardware behavior and cannot be fully disabled in firmware.

This is intentionally not nRF System OFF. System OFF is lower power, but it
cannot wake every minute from RTC on a bare nRF52840, so it would break pet
timers unless an external RTC wake circuit is added.

## Battery Monitoring

The hardware build checks the battery at startup and approximately every
60 seconds, including periodic MCU wakes while the pet is sleeping. It uses
`analogReadVDDHDIV5()` to read the nRF52840's internal VDDH/5 input; no extra
ADC wire is used. This assumes the board's **BAT+ feeds VDDH** when running
from the battery. Confirm this on the exact V1940 board revision before
relying on the readings; a regulated 3.3 V VDDH supply cannot report cell voltage.
The root/Wokwi sketch does not implement this monitor.

- At or below an estimated **40%**, show **BATTERY / 40%**.
- At or below an estimated **20%**, show **BATTERY / 20% / PLEASE CHARGE**.
- Left, Select, Right, or Mute dismisses the alert. That press is consumed;
  release the button before using it for gameplay or holding Select to wake.
- The warning remains until dismissed, or a 20% warning replaces a 40% warning.
  A battery already below 20% at boot shows only the 20% warning.
- Alerts use the selected English, Bulgarian, or German language. They do not
  play a melody. Pet timers and sleep/hospital protection continue underneath.
- Each threshold alerts once until a valid battery reading rises above 45%
  or 25%, respectively. This margin prevents repeated warnings near a threshold.
  Alert history is held in RAM, so a restart can show the warning again.

USB VBUS detection suppresses measurements because USB can raise VDDH above
the battery voltage. After unplugging, allow at least 5 seconds before the
next reading (up to the next periodic MCU wake). Connecting USB alone does
not re-arm alerts or dismiss an existing warning. Readings use a calibrated
12-bit ADC, the internal 0.6 V reference with gain 1/2, 40 us acquisition time,
and averaged samples. Values outside 2.5-4.5 V are ignored.

**Percentage is an estimate, not a calibrated fuel gauge.** The provisional
single-cell 3.7 V Li-ion curve in [battery_monitor.h](battery_monitor.h) places
40% near **3.80 V** and 20% near **3.70 V**. The exact Nokia model and its
specified maximum charging voltage are still needed to calibrate this curve;
"3.7-4.4 V" alone does not establish its discharge profile. Measurements can
cover 4.4 V, but the provisional curve saturates at 100% from 4.2 V upward.
Firmware does not change the board charger's voltage or implement battery
shutdown. The pet's energy bar remains unrelated to the battery.

The input choice is supported by [Nordic's VDDHDIV5 guidance](https://devzone.nordicsemi.com/f/nordic-q-a/46513/how-to-measure-battery-voltage-4-2v-3-0v-without-external-resistors-on-nrf52840).
The board assumption and USB exclusion follow this [SuperMini reverse-engineered schematic](https://github.com/sasodoma/nrf52840-promicro#schematic),
which is a reference for similar boards, not verification of this physical PCB.

To validate on hardware, compare VDDH readings with a meter on BAT+/GND with
USB disconnected. Check alerts near 3.80 V and 3.70 V using a controlled
battery substitute, test each dismissal button, then confirm USB does not
produce a false high reading. Do not connect a bench supply and cell together.

## Sound

The [melody catalog](../../README.md#melodies-and-sound-effects) lists all nine
active melodies and their triggers. This build also enables short selection,
animation, hatching, and loss chirps that are silent stubs in the root sketch.
`happyTune()` remains empty in both builds. Hardware shifts sound frequencies
into 2000-4000 Hz and drives the buzzer pin LOW when silent. Mute suppresses
all playback and is persisted; in hospital the Mute button is used for the
recovery shortcut instead.

## Save System

Two logical records with sequence numbers and CRC32 are stored inside the
512-byte LittleFS file `/tamagochi.sav`. On boot, the newest valid record is
selected. State includes the pet, clock, language, mute setting, the
sleep-until-woken state, hospital timer, post-sleep grace counter, and firmware
build ID.

The sleep-state field keeps the former countdown field's size and position.
A compatible save with a nonzero old countdown now resumes sleep until the
user wakes the pet; no save-format version change is needed for this behavior.

`loadGame()` requires the current save version and matching build ID.
`build-nrf.ps1` regenerates `build_stamp.h` every build, so installing a newly
built firmware restarts initial setup rather than resuming the old pet.
Restarting the same firmware can resume a compatible save. Older save versions
are rejected before the legacy migration code can load them.

The storage wrapper deletes `/tamagochi.sav` before writing both slots again.
Therefore independent-slot power-loss protection is not guaranteed. The
record verification reads the RAM buffer, and the caller does not check the
file commit result; successful in-memory verification does not prove the
save reached flash.

The game clock resumes from its saved value and does not catch up time spent
completely powered off. RTC2 is used for periodic wake during System ON sleep.
