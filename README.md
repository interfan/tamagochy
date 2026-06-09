# Arduino Tamagotchi

A virtual pet for an Arduino Mega 2560 with a 1.54-inch e-paper screen. It has
an animated egg and hatch sequence, three original animal designs, care
activities, music, two minigames, a clock/date setup, and EEPROM saves.

## Parts

- Arduino Mega 2560
- Waveshare-style 1.54-inch V2 200x200 black/white e-paper module
- 3 tactile switches
- Passive buzzer
- Jumper wires and breadboard

The e-paper screen displays the pet, its age, and status bars. The built-in LED
also lights while the pet is sleeping.

## Libraries

Install these libraries using Arduino IDE's Library Manager:

- `GxEPD2` by Jean-Marc Zingg
- `Adafruit GFX Library` by Adafruit

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

Set the time, set the date, and choose a cat, dog, or bunny. An egg then
appears and hatches after a random **2 to 5 hours** while the Mega is powered.
The egg periodically changes pose, followed by an animated musical hatch.

## Home Screen

The top layout shows the clock, date, and care stats. The bottom layout is an
icon carousel. Use Left and Right to choose an icon and Select to use it.

Available actions:

- Food and water
- Play: `Guess Side` and `Stop Bar` minigames
- Nap and overnight sleep
- Clean poop and give medicine
- Teach/learn
- Pet, groom, and wash
- Options: set clock, set date, or choose a new egg

Each care action has its own short e-paper animation and buzzer sound.

## Clock Limitation

The project currently uses a software clock because the listed hardware does
not include a real-time clock module. Time and hatch progress advance only
while the Mega is powered. EEPROM stores the latest hourly state, but cannot
measure time while power is disconnected. Add a DS3231 RTC module for a clock
and hatch timer that continue while powered off.

## Different 1.54-Inch Panels

The sketch targets the common Waveshare 1.54-inch V2 B/W panel using the
`GxEPD2_154_D67` driver. If your panel has another controller or supports red,
change the display driver declaration near the top of `Tamagochi.ino` to the
matching GxEPD2 example declaration.
