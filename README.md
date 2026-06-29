# Tamagochi

An Arduino-style virtual pet firmware with hand-drawn 1-bit animal art, care
actions, mini games, sounds, persistent saves, and a Wokwi preview build.

The current simulator target is an ESP32 DevKit with an ILI9341 LCD preview.
The physical display path uses a 1.54-inch black/white e-paper driver. The
planned final hardware work is nRF52840 plus e-ink, but the real nRF52840 sleep
and board pin mapping still need final integration.

## Current Features

- 9 animals: cat, dog, bunny, panda, dragon, fox, pig, hamster, penguin
- Egg, hatch animation, home screen, care actions, mini games, hospital, grown-up ending
- Adult transition after 25 days
- English, Bulgarian, and German UI text
- Bulgarian uses Cyrillic font rendering
- Mute button with persisted sound setting
- Robust two-slot save system with CRC and sequence number
- Automated bitmap validation before Wokwi builds
- Low-power Wokwi preview loop: active for 60 seconds after input, then 60-second wake checks
- Wokwi build script that enforces a 1 MiB no-Bluetooth nRF52840-style flash budget

## Controls

| Control | Main use |
| --- | --- |
| Left | Previous action / previous menu item |
| Select | Confirm / perform action |
| Right | Next action / next menu item |
| Mute | Toggle sound on/off |

Wokwi pins:

| Control | ESP32 pin |
| --- | --- |
| Left | GPIO25 |
| Select | GPIO26 |
| Right | GPIO27 |
| Mute | GPIO14 |
| Buzzer | GPIO32 |

Legacy non-Wokwi pins:

| Control | Pin |
| --- | --- |
| Left | D2 |
| Select | D3 |
| Right | D4 |
| Mute | D5 |
| Buzzer | D8 |

## Wokwi Build

Run:

```powershell
.\build-wokwi.ps1
```

or:

```cmd
build-wokwi.cmd
```

Then start the simulator with:

```text
Wokwi: Start Simulator
```

If Wokwi says the firmware binary is missing, run the build script first. The
script generates:

```text
.wokwi-build/Tamagochi.ino.bin
```

The build script validates bitmap dimensions/RLE payloads, copies the sketch and
generated bitmap headers into `.wokwi-sketch`, compiles for `esp32:esp32:esp32`
with `-DWOKWI_SIM`, and checks the compiled flash size against a 1 MiB budget.

## Gameplay

On first start:

1. Choose language.
2. Set the clock.
3. Choose an animal.
4. Wait for the egg to hatch.

The egg hatches after a random 2 to 5 hours. In Wokwi, pressing Select three
times quickly on the egg screen forces hatching.

Home screen meters:

- Food
- Water
- Happiness
- Energy

Internal stats also include:

- Health
- Learning
- Poop
- Dirty
- Sick
- Virus level
- Age days

Available actions:

- Food
- Water
- Play
- Nap
- Overnight sleep
- Clean
- Medicine
- Read
- Pet
- Groom
- Bath

Play opens a mini-game menu:

- Higher / Lower
- Coin Toss
- Shell Game

## Stat Logic

Needs update every 20 minutes.

Normal awake drain:

- Food: `-4` per needs tick
- Water: average about `-3.33` per needs tick
- Happiness: `-3` per needs tick
- Energy: `-3` per needs tick

Approximate drain from 100 to 0:

| Stat | Time |
| --- | --- |
| Food | about 8h 20m |
| Water | about 10h |

When sleeping:

- Energy increases by `+8` per needs tick
- Food drains more slowly
- Water still drains

If energy reaches 0, the pet is forced to sleep for 12 hours.

If food is 0:

- Happiness becomes 0
- Power drains faster
- Health reaches hospital threshold after about 5 hours

If water is 0:

- Happiness becomes 0
- Power drains faster
- Health reaches hospital threshold after about 3 hours

Every 4 hours away from care:

- Food loses `18`

Dirty behavior:

- Eating increases dirty
- Drinking increases dirty
- Cleaning poop increases dirty
- Random dirt can appear over time
- Dirty at 50% lowers happiness
- Dirty at 100% limits happiness to 20

Virus chance increases from:

- 2 or more poop
- Dirty 50%+
- Food below 25%
- Water below 25%

Medicine clears sickness and reduces dirt. Bath/groom clear dirt and add a
temporary recovery bonus that softens sickness risk.

## Hospital

The pet enters hospital if health reaches 0.

Hospital duration:

```text
24 hours
```

While in hospital:

- Animal is inactive
- Timer is shown
- Wokwi low-power loop still wakes every minute to refresh the timer

After recovery:

- Food is restored to at least 50
- Water is restored to at least 50
- Happiness is restored to at least 35
- Energy is restored to at least 60
- Health is restored to at least 60
- Poop, dirt, sickness, and virus are cleared

## Grown-Up Ending

Each animal grows up after:

```text
25 days
```

The grown-up screen appears and Select returns to animal selection for a new pet.

Wokwi shortcut:

```text
5 Right presses, then 5 Left presses from Home
```

## Test Shortcuts

From Home:

| Shortcut | Result |
| --- | --- |
| 10 Right presses | Enter hospital |
| 10 Left presses | Show all status overlays: dirt, poop, viruses |
| 5 Left presses, then 5 Right presses | Open debug stats screen |

From Hospital:

| Shortcut | Result |
| --- | --- |
| 10 Right presses | Set hospital timer to 1 minute |

## Save System

The firmware uses robust dual-slot saving.

Each save record contains:

- Record magic
- Sequence number
- Save data
- CRC32

There are two save slots. Each new save writes to the opposite slot. On boot,
the firmware loads the newest valid slot. If power fails during a write and the
new slot has a bad CRC, the previous slot is still valid.

Legacy single-slot saves are migrated automatically on first load.

Saved state includes:

- Clock
- Pet stats
- Animal
- Current stage
- Egg hatch timer
- Language
- Forced sleep timer
- Away hunger timer
- Empty food/water survival timers
- Attention timer
- Recovery bonus timer
- Hospital timer
- Virus level
- Mute setting

## Low Power

Current Wokwi behavior:

- After button/action activity, firmware stays active for 60 seconds.
- After that, ESP32 light sleep is used in Wokwi.
- It wakes every 60 seconds to check timers/events.
- Buttons can wake it.
- Display is hibernated after refresh.
- Idle pet animation and low-status flashing stop outside the active window.

Important:

The Wokwi sleep code is ESP32-specific preview code. Real nRF52840 hardware
still needs a proper System ON sleep implementation with RTC wake and GPIO
button wake.

## Display Notes

Wokwi uses ILI9341 as a fast visual preview. Real e-paper behaves differently:

- It keeps the last image without power.
- It consumes meaningful power mostly during refresh.
- Frequent refreshes can cause ghosting and visible flashing.

For final e-paper hardware, avoid frequent idle animation refreshes. Keep action
animations after user input, then return to a clean home refresh.

## Hardware Notes

The non-Wokwi display declaration currently targets:

```cpp
GxEPD2_154_D67
```

Legacy e-paper pins:

| E-paper pin | Pin |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| DIN / MOSI | D51 |
| CLK / SCK | D52 |
| CS | D53 |
| DC | D49 |
| RST | D48 |
| BUSY | D47 |

Use 3.3V logic unless your module explicitly supports level shifting.

For 2x AAA alkaline, do not connect to Li-ion charger pads. Power through the
safe board input for the chosen final PCB. If using a SuperMini nRF52840 board,
verify its exact power path before connecting batteries and USB together.

## Generated Assets

Important generated headers:

- `companion_bitmaps.h`
- `animal_idle_variants.h`
- `action_icons.h`
- `status_bitmaps.h`
- `species_action_bitmaps.h`

Idle variant generation:

```powershell
py -3 .\tools\make_idle_variants.py
```

Bitmap validation:

```powershell
py -3 .\tools\check_bitmaps.py
```

Preview assets are stored under:

```text
assets/bitmap-previews
assets/pixel-final
```

## Known Next Improvements

- Split the large sketch into modules.
- Add simulation tests for stat drain, hospital, grown-up transition, and save recovery.
- Add final nRF52840 sleep implementation.
- Add final battery voltage monitor once the final power path and ADC pin are chosen.
- Reduce real e-paper refresh frequency for final hardware.
