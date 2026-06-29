# TamagochiNrfEink

Standalone physical-device build for nRF52840 plus a 1.54-inch black/white
e-paper display. This folder is not for Wokwi.

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
- An nRF52840 Arduino core with EEPROM emulation

The sketch expects an EEPROM-style flash API with:

```cpp
EEPROM.begin(size);
EEPROM.get(address, value);
EEPROM.put(address, value);
EEPROM.commit();
```

## Default Pins

Buttons connect from pin to `GND`; the sketch uses `INPUT_PULLUP`.

| Signal | Default pin |
| --- | --- |
| Left | D2 |
| Select | D3 |
| Right | D4 |
| Mute | D5 |
| Buzzer | D6 |
| E-paper CS | D10 |
| E-paper DC | D9 |
| E-paper RST | D8 |
| E-paper BUSY | D7 |
| E-paper MOSI | board SPI MOSI |
| E-paper SCK | board SPI SCK |
| E-paper VCC | 3V3 |
| E-paper GND | GND |

If your SuperMini variant labels pins differently, edit the `#define *_PIN`
section near the top of `TamagochiNrfEink.ino`.

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

## Sleep Behavior

This build uses real nRF System ON sleep:

- E-paper is hibernated after every refresh.
- After 60 seconds without input, the MCU sleeps.
- RTC2 wakes the MCU every 60 seconds to update timers/stats.
- Button GPIO wake exits sleep immediately.
- Hospital timer refreshes once per minute.
- Idle animation does not auto-refresh on e-paper.

This is intentionally not nRF System OFF. System OFF is lower power, but it
cannot wake every minute from RTC on a bare nRF52840, so it would break pet
timers unless an external RTC wake circuit is added.

## Save System

The same robust dual-slot save system is included:

- two flash/EEPROM slots
- sequence number
- CRC32
- previous valid slot survives if power dies during save
