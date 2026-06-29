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
- `build-nrf.ps1` - optional Arduino CLI wrapper

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

## Sleep Behavior

This build uses real nRF System ON sleep:

- E-paper is hibernated after every refresh.
- The V1940/nice!nano-compatible VCC switch on raw `P0.13` is held HIGH while peripherals are powered and driven LOW after e-paper refresh.
- Programmable indicator candidates `P0.15`, `P0.16`, `P1.10`, and `P1.15` are forced HIGH/off.
- After 60 seconds without input, the MCU sleeps.
- RTC2 wakes the MCU every 60 seconds to update timers/stats.
- Button GPIO wake exits sleep immediately.
- Hospital timer refreshes once per minute.
- Idle animation does not auto-refresh on e-paper.

If a red/orange charge LED remains on while USB is connected, that is charger
hardware behavior and cannot be fully disabled in firmware.

This is intentionally not nRF System OFF. System OFF is lower power, but it
cannot wake every minute from RTC on a bare nRF52840, so it would break pet
timers unless an external RTC wake circuit is added.

## Save System

The same robust dual-slot save system is included:

- two flash/EEPROM slots
- sequence number
- CRC32
- previous valid slot survives if power dies during save
