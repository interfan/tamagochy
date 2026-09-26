# Tamagochi

An Arduino-style virtual pet firmware with hand-drawn 1-bit animal art, care
actions, mini games, sounds, persistent saves, and a Wokwi preview build.

The repository contains two separate firmware sketches:

- [`Tamagochi.ino`](Tamagochi.ino): ESP32 DevKit / Wokwi with an ILI9341 LCD
  preview, plus a legacy non-Wokwi e-paper path.
- [`output/TamagochiNrfEink`](output/TamagochiNrfEink/README.md): physical
  nice!nano / Pro Micro-compatible nRF52840 with a 1.54-inch black/white
  e-paper display. Board pin mapping, RTC2/GPIO wake, System ON sleep, and
  partial display refresh are implemented here.

The sketches share most gameplay and bitmap assets but have different sleep,
sound, save, and display behavior. Hardware-specific differences are called
out below. The `output` sketch is maintained separately from the root sketch.

## Current Features

- 9 animals: cat, dog, bunny, panda, dragon, fox, pig, hamster, penguin
- Egg, hatch animation, home screen, care actions, mini games, hospital, grown-up ending
- Adult transition after 25 days
- English, Bulgarian, and German UI text
- Bulgarian uses Cyrillic font rendering
- Mute button with persisted sound setting
- Two-slot save records with CRC and sequence number; storage limitations below
- Hardware-only pet information screen with age and calculated weight
- Hardware-only sleep until the user wakes the pet, with hospital protection
  during deep sleep and for one hour afterward
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

In the hardware build, Mute is repurposed while in hospital: 12 presses recover
the pet immediately. Holding Select for 5 seconds wakes the pet from Overnight
or exhaustion sleep, including after restarting the same firmware. See the
hardware README for its separate pin map.

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

The egg hatches after a random 2 to 5 hours. In both builds, pressing Select
three times with no more than 700 ms between presses forces hatching.

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

A win adds 20 happiness and a loss adds 5; either result costs 8 food and
8 energy. The hardware build adds an Info entry showing age out of 25 days
and weight calculated from species, age, and current condition.

Excellent care triggers an "I love you" screen and melody: the pet must be
awake, clean, free of poop and sickness/viruses, have food/water/energy above
50, and happiness at 100. The message closes after 60 seconds or button input.

Reading increases learning, but learning currently has no unlocks or other
gameplay effects. Settings code exists for changing the clock and starting a
new egg, but normal HOME navigation excludes the Settings action, so that menu
is currently unreachable.

The clock advances from firmware timers and resumes from its saved value after
a restart. Elapsed time while completely powered off is not recovered. The
DS3231 in `diagram.json` is not read by the firmware.

## Stat Logic

Needs update every 20 minutes.

Normal awake drain:

- Food: `-4` per needs tick
- Water: average about `-3.33` per needs tick
- Happiness: `-3` per needs tick
- Energy: `-3` per needs tick

Approximate base drain from 100 to 0, excluding actions and the extra away
hunger penalty:

| Stat | Time |
| --- | --- |
| Food | about 8h 20m |
| Water | about 10h |

When sleeping:

- Energy increases by `+8` per needs tick
- Food drains more slowly
- Water still drains

If energy reaches 0, the hardware build sleeps until the user holds Select for
5 seconds. The root/Wokwi build still uses 12-hour forced sleep.

The manual Overnight action differs between builds:

- Hardware: sets energy to 100, reduces food to at most 20, and remains asleep
  until Select is held for 5 seconds. There is no automatic wake after 12 hours.
  Exhaustion uses the same sleep scene and manual wake control. Short presses
  and Left/Right do not perform care actions during deep sleep; Mute still works.
  The sleep state is saved and restored. Age still advances, but the grown-up
  ending is deferred until the user wakes the pet. Nap remains a separate toggle.
- Root/Wokwi: sets sleeping, energy to 100, and food to at most 20, but does not
  start a new 12-hour timer. Its timed forced sleep is triggered by exhaustion.

If food is 0:

- Happiness becomes 0
- Energy drains faster while awake
- Health reaches hospital threshold after about 5 hours

If water is 0:

- Happiness becomes 0
- Energy drains faster while awake
- Health reaches hospital threshold after about 3 hours

Every 4 hours without button interaction while awake:

- Food loses `18`

Dirty behavior:

- Eating increases dirty
- Drinking increases dirty
- Cleaning poop increases dirty
- Random dirt can appear over time
- Dirty at 50% lowers happiness
- Adding dirt at 100% caps happiness at 20 at that moment; subsequent care can
  raise happiness again

Virus chance increases from:

- 2 or more poop
- Dirty 50%+
- Food below 25%
- Water below 25%

Medicine clears sickness and reduces dirt. Bath/groom clear dirt and add a
temporary recovery bonus that softens sickness risk.

## Hospital

The pet enters hospital when a needs update detects health at 0, subject to
the hardware build's sleep protection below.

### Protection during deep sleep and after waking (hardware build only)

`hospitalBlockedByDeepSleep()` blocks admission whenever
`deepSleepAwaitingWake` is set or `postDeepSleepHospitalGraceMinutes > 0`.
When the user wakes the pet, `wakeFromDeepSleep()` sets the grace counter to
`POST_DEEP_SLEEP_HOSPITAL_GRACE_MINUTES`, currently **60 game minutes**.
This applies to both exhaustion sleep and the manual Overnight action. The
counter starts on manual wake, decreases once per game minute, and is included
in saved state. Passing 12 hours alone never starts the grace countdown.

The protection delays hospital admission; needs and health can still fall
during sleep and the grace period. Waking does not restore health. Use the
hour to restore food/water and, if needed, health with Medicine or Bath.
Once the counter expires, a subsequent needs update can admit a pet whose
health is still 0. An ordinary Nap does not grant this grace period.

The root/Wokwi sketch has neither the hospital-admission guard nor the
post-sleep grace counter, so it can enter hospital during or after sleep.

Hospital duration:

```text
24 hours
```

While in hospital:

- Animal is inactive
- Timer is shown
- Wokwi low-power loop still wakes every minute to refresh the timer
- Hardware counts down every minute but displays remaining hours rounded up;
  its timer display changes once per hour

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

The HOME and Hospital shortcuts in the following tables are for the root
sketch. The hardware build instead has the 12-Mute-press hospital recovery
shortcut described under Controls.

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

Both sketches use two logical save slots.

Each save record contains:

- Record magic
- Sequence number
- Save data
- CRC32

Each new record targets the opposite slot. On boot, the firmware selects the
newest record with a valid header and CRC. A damaged record can be skipped if
the other record remains intact; this does not by itself guarantee that the
underlying storage preserves one slot during power loss.

The root sketch includes legacy single-slot migration. The hardware loader
requires the current save version and matching firmware build ID. Its build
script generates a new ID each build, so installing a newly built firmware
returns to initial setup instead of resuming the previous pet.

Hardware storage uses a 512-byte LittleFS file, `/tamagochi.sav`, containing
both slots. `NrfSaveStorage::commit()` deletes this file before rewriting it;
the slots are not independent files, so power-loss-safe replacement remains
an improvement to implement. The write verification reads the RAM buffer and
does not confirm that the file was successfully persisted.

Saved state includes:

- Clock
- Pet stats
- Animal
- Current stage
- Egg hatch timer
- Language
- Root forced-sleep timer / hardware sleep-until-woken state
- Away hunger timer
- Empty food/water survival timers
- Attention timer
- Recovery bonus timer
- Hospital timer
- Virus level
- Mute setting
- Hardware-only firmware build ID and post-sleep hospital grace counter

## Low Power

Current Wokwi behavior:

- After button/action activity, firmware stays active for 60 seconds.
- After that, ESP32 light sleep is used in Wokwi.
- It wakes every 60 seconds to check timers/events.
- Buttons can wake it.
- The shared code calls display hibernation after refresh, but the Wokwi LCD
  adapter implements it as a no-op.
- Idle pet animation and low-status flashing stop outside the active window.

The Wokwi sleep code is ESP32-specific preview code. The separate hardware
sketch implements nRF52840 System ON sleep with RTC2 wake every 60 seconds,
GPIO button wake, display hibernation, and peripheral power switching.
Its 60-second active window is separate from the pet's sleep state; periodic
MCU wake checks do not wake a sleeping pet.

## Display Notes

Wokwi uses ILI9341 as a fast visual preview. Real e-paper behaves differently:

- It keeps the last image without power.
- It consumes meaningful power mostly during refresh.
- Frequent refreshes can cause ghosting and visible flashing.

The hardware build currently animates HOME every 10 seconds during the active
window, then stops idle animation when inactive. Low-status flashing is
disabled there; the Wokwi build enables flashing and uses a 6-second idle
animation interval. The hardware build uses partial refreshes where possible;
see its README for the full/partial refresh rules.

## Battery Monitoring

The **nRF52840 hardware build** now checks the internal VDDH/5 input at
startup and approximately every minute while running on battery. This assumes
BAT+ supplies VDDH on the actual board. USB-powered readings are skipped.

At an estimated 40%, a dismissible **BATTERY / 40%** screen appears. At 20%,
it shows **BATTERY / 20% / PLEASE CHARGE**. Any of the four buttons dismisses
it without also performing its usual action. The 20% warning takes priority
if the battery is already low at boot. Alerts do not repeat until charge rises
above the threshold plus a 5-point margin, or the device restarts. Game timers
continue, and dismissing an alert does not wake a deeply sleeping pet.

The provisional 3.7 V single-cell Li-ion curve uses approximately 3.80 V for
40% and 3.70 V for 20%. It needs calibration against the exact Nokia battery
model; the stated "3.7-4.4 V" range is not enough to determine state of charge.
See [hardware battery monitoring](output/TamagochiNrfEink/README.md#battery-monitoring)
for wiring assumptions, ADC settings, USB behavior, and validation steps.

The root/Wokwi build still has no battery monitor. There is no battery melody
or battery-triggered shutdown. The HOME energy meter measures pet energy.

## Melodies and Sound Effects

Both sketches define the same nine non-empty melodies. The frequencies below
are the base values in the note arrays; each note is followed by a 35 ms gap.

| Function | Base notes (Hz) | Where it plays |
| --- | --- | --- |
| `hatchTune()` | 523, 523, 587, 523, 698, 659 | At the end of egg hatching |
| `deepSleepTune()` | 523, 523, 784, 784, 880, 880, 784 | When the player chooses Overnight; not when exhaustion automatically starts sleep |
| `coinTune()` | 1175, 1568 | Normal care-action confirmation, unless a status alert takes priority; not specifically a Coin Toss reward |
| `unmuteTune()` | 880, 1175 | When sound is switched back on |
| `badStatusTune()` | 587, 554, 587, 494 | Newly low food/water/happiness/energy, dirt threshold crossings, new poop, or new/worsening illness; also the root status-overlay test shortcut |
| `hospitalTune()` | 784, 523, 784, 523, 784, 523 | On entering hospital |
| `loveTune()` | 784, 988, 1175, 1568, 1175 | When the excellent-care love message appears |
| `lifeUpTune()` | 659, 784, 988, 1319, 1568, 1760 | Winning any mini-game, unless that round triggers a low-status alert |
| `grownUpTune()` | 659, 659, 698, 784, 784, 698, 659, 587, 523, 523, 587, 659, 659, 587, 587 | On reaching the grown-up ending |

`happyTune()` is called when a new egg starts, but its body is empty in both
sketches, so it produces no sound.

The hardware build also plays short `chirp()` effects:

- HOME action selection: 900 Hz for 25 ms.
- Four care-animation frames: 650, 830, 1010, 1190 Hz, each for 70 ms.
- Four hatching frames: 500, 680, 860, 1040 Hz, each for 160 ms.
- A lost mini-game: 300 Hz for 160 ms, unless a low-status alert takes priority.

The root sketch's `chirp()` is an empty stub, so these short effects are silent
in Wokwi; the named melodies still play. Hardware maps all base frequencies
into the 2000-4000 Hz range through `loudBuzzerFrequency()`, so its pitch differs
from Wokwi. Sound is globally muted by `soundMuted`, and the preference is
saved. Overnight can play its melody followed by a status alert, and hardware
care-animation chirps follow the action melody/alert. There is no background
music, battery melody, or separate wake-up/recovery melody.

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
- Keep the root and hardware gameplay behavior aligned, especially sleep protection.
- Make Settings reachable through normal navigation.
- Make hardware save replacement safe across power loss and preserve saves across builds.
- Validate the hardware VDDH battery reading and calibrate the discharge curve for the exact Nokia cell.
- Reduce real e-paper refresh frequency for final hardware.
