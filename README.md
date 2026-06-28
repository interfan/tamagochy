# Arduino Tamagotchi

A small original companion game for an Arduino Mega 2560 with a 1.54-inch
e-paper screen. It has
an animated egg and hatch sequence, nine original animal designs, care
activities, music, two minigames, a clock setup, and EEPROM saves.

At startup, choose English, Bulgarian, or German. The selected language is
remembered, and Bulgarian text uses a proper Cyrillic display font.

## Parts

- Arduino Mega 2560
- Waveshare-style 1.54-inch V2 200x200 black/white e-paper module
- 3 tactile switches
- Passive buzzer
- Jumper wires and breadboard

The e-paper screen displays the pet, its age, and status bars. The built-in LED
also lights while the pet is sleeping.

The physical e-paper build uses partial refreshes for clock ticks, status
changes, action-menu navigation, and egg movement. Screen transitions remain
full refreshes, and a full cleanup refresh runs automatically after several
partial updates to limit ghosting.

During first-time setup, language changes, clock edits, and animal
selection use partial refreshes where possible. The first screen and the final
transition to the egg still use clean full refreshes.

Clock Left and Right edits refresh only the number card. Confirming a
field refreshes the heading, number, and instruction area. Hatch animation
frames also use partial updates, followed by a clean full Home refresh.

The onboarding screens use a restrained old-book design built on one measured
layout: double border, symmetrical curled corner flourishes, centered chapter heading,
diamond dividers, rectangular selection plates, and consistent baselines.

## Libraries

Install these libraries using Arduino IDE's Library Manager:

- `GxEPD2` by Jean-Marc Zingg
- `Adafruit GFX Library` by Adafruit
- `U8g2_for_Adafruit_GFX` by oliver

## Wiring

Each button connects between its Arduino pin and `GND`. The controls change
meaning depending on the screen:

- **Left:** previous icon or decrease a setup value
- **Select:** confirm or perform the selected action
- **Right:** next icon or increase a setup value

| Tactile switch | Mega 2560 pin |
| --- | --- |
| Left button | D2 |
| Select button | D3 |
| Right button | D4 |

Connect one leg of each switch to its listed pin and the opposite leg to
`GND`. No button resistors are required because the sketch uses `INPUT_PULLUP`.

### Buzzer

| Passive buzzer | Mega 2560 pin |
| --- | --- |
| Passive buzzer positive | D8 |
| Passive buzzer negative | GND |

### 1.54-Inch E-Paper

| E-paper pin | Mega 2560 pin |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| DIN / MOSI | D51 |
| CLK / SCK | D52 |
| CS | D53 |
| DC | D49 |
| RST | D48 |
| BUSY | D47 |

Use `3.3V` unless your particular module explicitly states that it accepts
`5V`. The pin mapping targets the Mega's hardware SPI interface.

## Run It

1. Open `Tamagochi.ino` in the Arduino IDE.
2. Install the required libraries using Library Manager.
3. Select your Arduino Mega 2560 and port.
4. Upload the sketch.
5. Optionally open Serial Monitor at **9600 baud** for diagnostics.

## First Start

Set the time and choose a cat, dog, bunny, panda, dragon, fox,
pig, hamster, or penguin. An egg then appears and hatches after a
random **2 to 5 hours** while the Mega is powered. The egg periodically changes
pose, followed by an animated musical hatch.

## Home Screen

The top layout shows icon-based care meters for food, water, happiness, and
energy. The bottom layout is an icon carousel. Use Left
and Right to choose an icon and Select to use it.

Available actions:

- Food and water
- Play: `Guess Side` and `Stop Bar` minigames
- Nap and overnight sleep
- Clean poop and give medicine
- Read
- Pet, groom, and bath

All care-action icons are visible together in the two-row footer. Only the
selected action has a border. Press **Left and Right together** from Home to
open Options for the clock or a new companion.

Each care action has its own short e-paper animation and buzzer sound. Every
animal has idle, blink, eating, happy, and sleeping artwork. The cat also uses
larger hand-drawn action scenes. The other animals use full feeding scenes
with species-specific food: bone, carrot, bamboo, meat, grain, apple, seeds,
or fish. They also have full drinking scenes with species-sized bowls and
details such as the dragon's stone basin and penguin's ice-rimmed dish. Their
sleep and overnight actions reuse species-specific scenes: nest, cave, igloo,
burrow, hammock, mud puddle, or pet bed. Medicine, pet, groom, clean, bath,
and read also use four-frame species scenes with a unique motif for every
animal.

## Clock Limitation

The project currently uses a software clock because the listed hardware does
not include a real-time clock module. Time and hatch progress advance only
while the Mega is powered. EEPROM stores the latest hourly state, but cannot
measure time while power is disconnected. Add a DS3231 RTC module for a clock
and hatch timer that continue while powered off.

## Wokwi VS Code Simulation

The repository includes `diagram.json` and `wokwi.toml`. Install the
**Wokwi Simulator** VS Code extension, then:

1. Run `.\build-wokwi.cmd` in the VS Code terminal.
2. Press `F1`.
3. Run **Wokwi: Start Simulator**.

Wokwi uses the ESP32 DevKit C V4 and stock ILI9341 LCD as the preview target,
with the LCD reset line wired to GPIO4. The physical build continues to
compile for the 1.54-inch e-paper display. ESP32 is only the supported Wokwi
stand-in for the future nRF52840 hardware; the game code, buttons, buzzer,
menus, and animations are shared. Wokwi loads the Arduino ESP32 application
`.bin` generated by the build.

Every `build-wokwi.cmd` run measures the compiled ESP32 application's flash
usage and rejects builds above **1,048,576 bytes**, matching the raw flash
capacity of an nRF52840 when no Bluetooth SoftDevice is reserved. It also
reports the generated bitmap payload separately, making animation growth easy
to track. Framework overhead differs between ESP32 and nRF52840, so the final
nRF build must still be measured before hardware release.

On the egg screen, press **Select three times quickly** to hatch immediately.
This shortcut is compiled only into the Wokwi simulation firmware.

`diagram.json` also includes a `wokwi-ds3231` RTC module wired on ESP32 I2C
pins GPIO21/SDA and GPIO22/SCL. The RTC keeps simulation time moving while
the project is paused or restarted in Wokwi.

The original kawaii animal concept sheets used as drawing references are saved
under `assets/kawaii-*-companions.png`. Generated 1-bit previews are under
`assets/bitmap-previews`.

## Different 1.54-Inch Panels

The sketch targets the common Waveshare 1.54-inch V2 B/W panel using the
`GxEPD2_154_D67` driver. If your panel has another controller or supports red,
change the display driver declaration near the top of `Tamagochi.ino` to the
matching GxEPD2 example declaration.
