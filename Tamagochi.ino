/*
  E-Paper Tamagotchi

  Controls everywhere:
    D2 = LEFT / previous / decrease
    D3 = SELECT / confirm
    D4 = RIGHT / next / increase

  The physical build targets the Mega/e-paper wiring below. Wokwi uses a
  standard ESP32 DevKit as a 32-bit preview target for the future nRF52840.
*/

#include <Adafruit_GFX.h>
#include <EEPROM.h>
#include "companion_bitmaps.h"
#include "action_icons.h"
#include "species_action_bitmaps.h"
#ifdef WOKWI_SIM
#include <SPI.h>
#include <Adafruit_ILI9341.h>
#else
#include <GxEPD2_BW.h>
#endif

#if defined(__AVR__)
typedef uint_farptr_t FlashAddress;
#define FLASH_ADDRESS(symbol) pgm_get_far_address(symbol)
#define READ_FLASH_BYTE(address) pgm_read_byte_far(address)
#else
typedef uintptr_t FlashAddress;
#define FLASH_ADDRESS(symbol) reinterpret_cast<FlashAddress>(symbol)
#define READ_FLASH_BYTE(address) pgm_read_byte(reinterpret_cast<const uint8_t *>(address))
#endif

#ifdef WOKWI_SIM
const byte LEFT_PIN = 25;
const byte SELECT_PIN = 26;
const byte RIGHT_PIN = 27;
const byte BUZZER_PIN = 32;
const byte EPD_CS_PIN = 15;
const byte EPD_DC_PIN = 2;
const byte EPD_RST_PIN = 4;
const byte EPD_BUSY_PIN = 33;
#else
const byte LEFT_PIN = 2;
const byte SELECT_PIN = 3;
const byte RIGHT_PIN = 4;
const byte BUZZER_PIN = 8;
const byte EPD_CS_PIN = 53;
const byte EPD_DC_PIN = 49;
const byte EPD_RST_PIN = 48;
const byte EPD_BUSY_PIN = 47;
#endif

const unsigned long DEBOUNCE_MS = 35;
const unsigned long CLOCK_TICK_MS = 60000UL;
const unsigned long NEEDS_TICK_MS = 20UL * 60000UL;
const unsigned long EGG_FRAME_MS = 15UL * 60000UL;
const uint32_t SAVE_MAGIC = 0x54414D41UL;

#ifdef WOKWI_SIM
#define GxEPD_BLACK ILI9341_BLACK
#define GxEPD_WHITE ILI9341_WHITE

class WokwiDisplay : public Adafruit_ILI9341 {
 public:
  WokwiDisplay(byte cs, byte dc, byte rst) : Adafruit_ILI9341(cs, dc, rst) {}
  void init(unsigned long) { begin(); }
  void setFullWindow() {}
  void setPartialWindow(uint16_t, uint16_t, uint16_t, uint16_t) {}
  void firstPage() {}
  bool nextPage() { return false; }
  void hibernate() {}
};

WokwiDisplay display(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN);
#else
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));
#endif

enum Screen : byte {
  LANGUAGE = 0, SET_CLOCK = 1, SELECT_ANIMAL = 3, EGG = 4, HATCHING = 5,
  HOME = 6, ACTION_SCENE = 7, GAME_MENU = 8, OPTIONS = 9
};
enum Animal : byte { CAT, DOG, BUNNY, PANDA, DRAGON, FOX, CHICKEN, PIG, HAMSTER, PENGUIN, ANIMAL_COUNT };
enum AnimalPose : byte { POSE_IDLE, POSE_HAPPY, POSE_SLEEP };
enum Action : byte {
  FEED, WATER, PLAY, SLEEP, OVERNIGHT, CLEAN, MEDICINE,
  LEARN, PET_ACTION, GROOM, WASH, SETTINGS, ACTION_COUNT
};

struct Button {
  byte pin;
  bool stable;
  bool last;
  unsigned long changedAt;
};

struct ClockData {
  byte hour;
  byte minute;
  byte day;
  byte month;
  unsigned int year;
};

struct Pet {
  byte food;
  byte water;
  byte happy;
  byte energy;
  byte health;
  byte learning;
  byte poop;
  bool dirty;
  bool sick;
  bool sleeping;
  unsigned int ageDays;
};

struct SaveData {
  uint32_t magic;
  ClockData clock;
  Pet pet;
  byte animal;
  byte stage;
  unsigned int hatchMinutesLeft;
};

Button leftButton = {LEFT_PIN, HIGH, HIGH, 0};
Button selectButton = {SELECT_PIN, HIGH, HIGH, 0};
Button rightButton = {RIGHT_PIN, HIGH, HIGH, 0};
ClockData gameClock = {12, 0, 1, 1, 2026};
Pet pet = {80, 80, 80, 80, 100, 0, 0, false, false, false, 0};

Screen screen = SET_CLOCK;
Animal animal = CAT;
Action selectedAction = FEED;
Action sceneAction = FEED;
byte editField = 0;
byte animalChoice = 0;
byte gameChoice = 0;
byte eggFrame = 0;
byte sceneFrame = 0;
unsigned int hatchMinutesLeft = 0;
unsigned long lastClockTick = 0;
unsigned long lastNeedsTick = 0;
unsigned long lastEggFrame = 0;
bool displayDirty = true;
bool setupCreatesEgg = true;
byte languageChoice = 0;
bool startupShortcutArmed = true;
unsigned long startupSelectHeldSince = 0;
byte eggSelectCount = 0;
unsigned long lastEggSelect = 0;

byte clampStat(int value) {
  return constrain(value, 0, 100);
}

bool leapYear(unsigned int year) {
  return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
}

byte daysInMonth(byte month, unsigned int year) {
  const byte days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && leapYear(year) ? 29 : days[month - 1];
}

void chirp(unsigned int frequency, unsigned int duration) {
  tone(BUZZER_PIN, frequency, duration);
}

void playTune(const int *notes, const uint16_t *lengths, byte count) {
  for (byte i = 0; i < count; i++) {
    tone(BUZZER_PIN, notes[i], lengths[i]);
    delay(lengths[i] + 35);
  }
  noTone(BUZZER_PIN);
}

void happyTune() {
  const int notes[] = {523, 659, 784};
  const uint16_t lengths[] = {90, 90, 150};
  playTune(notes, lengths, 3);
}

void hatchTune() {
  const int notes[] = {523, 659, 784, 1047, 784, 1047};
  const uint16_t lengths[] = {100, 100, 100, 220, 100, 300};
  playTune(notes, lengths, 6);
}

bool pressed(Button &button) {
  bool reading = digitalRead(button.pin);
  if (reading != button.last) {
    button.last = reading;
    button.changedAt = millis();
  }
  if (millis() - button.changedAt > DEBOUNCE_MS && reading != button.stable) {
    button.stable = reading;
    return reading == LOW;
  }
  return false;
}

void saveGame(byte stage) {
  SaveData data = {SAVE_MAGIC, gameClock, pet, (byte)animal, stage, hatchMinutesLeft};
  EEPROM.put(0, data);
#if defined(ESP32)
  EEPROM.commit();
#endif
}

bool loadGame() {
  SaveData data;
  EEPROM.get(0, data);
  if (data.magic != SAVE_MAGIC || data.animal >= ANIMAL_COUNT || data.stage > HOME) {
    return false;
  }
  gameClock = data.clock;
  pet = data.pet;
  animal = (Animal)data.animal;
  hatchMinutesLeft = data.hatchMinutesLeft;
  screen = data.stage == EGG ? EGG : HOME;
  return true;
}

const __FlashStringHelper *animalName(Animal kind) {
  switch (kind) {
    case CAT: return F("MICA");
    case DOG: return F("PIP");
    case BUNNY: return F("LUMA");
    case PANDA: return F("PO");
    case DRAGON: return F("EMBER");
    case FOX: return F("FEN");
    case CHICKEN: return F("PIPPI");
    case PIG: return F("TRUFFLE");
    case HAMSTER: return F("NIBBLE");
    case PENGUIN: return F("PIPER");
    default: return F("FRIEND");
  }
}

void drawCentered(const __FlashStringHelper *text, int y, byte size = 1) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((200 - w) / 2, y);
  display.print(text);
}

void drawCenteredInBox(const __FlashStringHelper *text, int x, int y, int w, int h, byte size = 1) {
  int16_t x1, y1;
  uint16_t tw, th;
  display.setTextSize(size);
  display.getTextBounds(text, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor(x + (w - tw) / 2 - x1, y + (h - th) / 2 - y1);
  display.print(text);
}

void drawScaledBitmap(int x, int y, FlashAddress bitmap, int width, int height, int scalePercent) {
  int bytesPerRow = (width + 7) / 8;
  for (int sourceY = 0; sourceY < height; sourceY++) {
    int outputY = y + sourceY * scalePercent / 100;
    int outputBottom = y + (sourceY + 1) * scalePercent / 100;
    int runStart = -1;
    for (int sourceX = 0; sourceX <= width; sourceX++) {
      bool black = false;
      if (sourceX < width) {
        uint8_t value = READ_FLASH_BYTE(bitmap + sourceY * bytesPerRow + sourceX / 8);
        black = value & (0x80 >> (sourceX % 8));
      }
      if (black && runStart < 0) runStart = sourceX;
      if (!black && runStart >= 0) {
        int outputX = x + runStart * scalePercent / 100;
        int outputRight = x + sourceX * scalePercent / 100;
        display.fillRect(outputX, outputY, outputRight - outputX, outputBottom - outputY, GxEPD_BLACK);
        runStart = -1;
      }
    }
  }
}

void drawClock() {
  display.fillRoundRect(3, 2, 194, 18, 8, GxEPD_BLACK);
  display.setTextColor(GxEPD_WHITE);
  display.setTextSize(1);
  display.setCursor(8, 7);
  if (gameClock.hour < 10) display.print('0');
  display.print(gameClock.hour);
  display.print(':');
  if (gameClock.minute < 10) display.print('0');
  display.print(gameClock.minute);
  display.setCursor(122, 7);
  if (gameClock.day < 10) display.print('0');
  display.print(gameClock.day);
  display.print('.');
  if (gameClock.month < 10) display.print('0');
  display.print(gameClock.month);
  display.print('.');
  display.print(gameClock.year);
  const __FlashStringHelper *name = animalName(animal);
  int16_t x1, y1;
  uint16_t nameWidth, nameHeight;
  display.getTextBounds(name, 0, 7, &x1, &y1, &nameWidth, &nameHeight);
  display.setCursor(100 - nameWidth / 2, 7);
  display.print(name);
  display.setTextColor(GxEPD_BLACK);
}

void drawHeart(int x, int y, bool filled = true) {
  if (filled) {
    display.fillCircle(x + 3, y + 3, 3, GxEPD_BLACK);
    display.fillCircle(x + 9, y + 3, 3, GxEPD_BLACK);
    display.fillTriangle(x, y + 4, x + 12, y + 4, x + 6, y + 12, GxEPD_BLACK);
  } else {
    display.drawCircle(x + 3, y + 3, 3, GxEPD_BLACK);
    display.drawCircle(x + 9, y + 3, 3, GxEPD_BLACK);
  }
}

void drawStatusIcon(byte icon, int x, int y) {
  if (icon == 0) {
    display.drawCircle(x + 8, y + 6, 5, GxEPD_BLACK);
    display.drawCircle(x + 8, y + 6, 2, GxEPD_BLACK);
    display.drawLine(x, y + 1, x, y + 11, GxEPD_BLACK);
    display.drawLine(x + 2, y + 1, x + 2, y + 11, GxEPD_BLACK);
  } else if (icon == 1) {
    display.drawCircle(x + 8, y + 6, 4, GxEPD_BLACK);
    display.fillTriangle(x + 8, y, x + 5, y + 6, x + 11, y + 6, GxEPD_BLACK);
  } else if (icon == 2) {
    drawHeart(x, y - 1);
  } else {
    display.fillTriangle(x + 8, y, x + 3, y + 7, x + 8, y + 7, GxEPD_BLACK);
    display.fillTriangle(x + 7, y + 5, x + 12, y + 5, x + 5, y + 12, GxEPD_BLACK);
  }
}

void drawMeter(int x, int y, int w, byte icon, int value) {
  drawStatusIcon(icon, x, y - 2);
  display.drawRoundRect(x + 18, y - 1, w - 18, 7, 3, GxEPD_BLACK);
  display.fillRoundRect(x + 20, y + 1, map(value, 0, 100, 0, w - 22), 3, 1, GxEPD_BLACK);
}

void drawBookCorner(int x, int y, int sx, int sy) {
  display.drawLine(x, y, x + sx * 20, y, GxEPD_BLACK);
  display.drawLine(x, y, x, y + sy * 20, GxEPD_BLACK);
  display.drawCircle(x + sx * 12, y + sy * 12, 8, GxEPD_BLACK);
  display.drawCircle(x + sx * 12, y + sy * 12, 4, GxEPD_BLACK);
  display.fillCircle(x + sx * 12, y + sy * 12, 1, GxEPD_BLACK);
  display.fillEllipse(x + sx * 23, y + sy * 5, 5, 2, GxEPD_BLACK);
  display.fillEllipse(x + sx * 5, y + sy * 23, 2, 5, GxEPD_BLACK);
  display.drawLine(x + sx * 18, y + sy * 8, x + sx * 26, y + sy * 4, GxEPD_BLACK);
  display.drawLine(x + sx * 8, y + sy * 18, x + sx * 4, y + sy * 26, GxEPD_BLACK);
}

void drawBookFrame() {
  display.drawRect(4, 4, 192, 192, GxEPD_BLACK);
  display.drawRect(8, 8, 184, 184, GxEPD_BLACK);
  drawBookCorner(12, 12, 1, 1);
  drawBookCorner(188, 12, -1, 1);
  drawBookCorner(12, 188, 1, -1);
  drawBookCorner(188, 188, -1, -1);
}

void drawSetupFrame() {
  display.drawRoundRect(2, 2, 196, 196, 12, GxEPD_BLACK);
  display.drawRoundRect(6, 6, 188, 188, 10, GxEPD_BLACK);
  display.fillCircle(15, 15, 2, GxEPD_BLACK);
  display.fillCircle(185, 15, 2, GxEPD_BLACK);
  display.fillCircle(15, 185, 2, GxEPD_BLACK);
  display.fillCircle(185, 185, 2, GxEPD_BLACK);
}

void drawSetupArrows(int y) {
  display.fillTriangle(24, y, 34, y - 7, 34, y + 7, GxEPD_BLACK);
  display.fillTriangle(176, y, 166, y - 7, 166, y + 7, GxEPD_BLACK);
  display.drawCircle(100, y, 5, GxEPD_BLACK);
  display.fillCircle(100, y, 2, GxEPD_BLACK);
}

void drawBookDivider(int y) {
  display.drawLine(38, y, 82, y, GxEPD_BLACK);
  display.drawLine(118, y, 162, y, GxEPD_BLACK);
  display.fillTriangle(91, y, 100, y - 5, 109, y, GxEPD_BLACK);
  display.fillTriangle(91, y, 100, y + 5, 109, y, GxEPD_BLACK);
  display.fillCircle(100, y, 2, GxEPD_WHITE);
}

void drawBookHeading(const __FlashStringHelper *title, const __FlashStringHelper *subtitle) {
  drawBookFrame();
  drawCentered(title, 58, 2);
  drawBookDivider(68);
  drawCentered(subtitle, 90);
}

void drawBookChoice(int x, int y, int w, int h, const __FlashStringHelper *label, bool selected) {
  if (selected) {
    display.fillRoundRect(x, y, w, h, 8, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
  } else {
    display.drawRoundRect(x, y, w, h, 8, GxEPD_BLACK);
    display.setTextColor(GxEPD_BLACK);
  }
  drawCenteredInBox(label, x, y, w, h, h >= 34 ? 2 : 1);
  display.setTextColor(GxEPD_BLACK);
}

void drawLanguageScreen() {
  drawSetupFrame();
  drawBookChoice(15, 78, 50, 44, F("EN"), languageChoice == 0);
  drawBookChoice(75, 78, 50, 44, F("BG"), languageChoice == 1);
  drawBookChoice(135, 78, 50, 44, F("DE"), languageChoice == 2);
  drawSetupArrows(164);
}

void drawVirus(int x, int y) {
  display.drawCircle(x, y, 8, GxEPD_BLACK);
  display.fillCircle(x - 3, y - 2, 1, GxEPD_BLACK);
  display.fillCircle(x + 3, y - 2, 1, GxEPD_BLACK);
  display.drawLine(x - 3, y + 3, x + 3, y + 3, GxEPD_BLACK);
  for (byte i = 0; i < 8; i++) {
    float angle = i * PI / 4.0;
    int x1 = x + cos(angle) * 9;
    int y1 = y + sin(angle) * 9;
    int x2 = x + cos(angle) * 13;
    int y2 = y + sin(angle) * 13;
    display.drawLine(x1, y1, x2, y2, GxEPD_BLACK);
    display.fillCircle(x2, y2, 2, GxEPD_BLACK);
  }
}

void drawEgg(int x, int y, byte frame) {
  int lean = frame == 1 ? -4 : frame == 2 ? 4 : 0;
  drawScaledBitmap(x - EGG_WIDTH / 2 + lean, y - EGG_HEIGHT / 2,
                   FLASH_ADDRESS(EGG_BITMAP), EGG_WIDTH, EGG_HEIGHT, 100);
  display.drawLine(x - 16 + lean, y - 29, x - 9 + lean, y - 23, GxEPD_BLACK);
  display.drawLine(x - 11 + lean, y - 21, x - 4 + lean, y - 17, GxEPD_BLACK);
  display.drawLine(x + 7 + lean, y - 28, x + 13 + lean, y - 22, GxEPD_BLACK);
  display.drawLine(x + 11 + lean, y - 16, x + 17 + lean, y - 11, GxEPD_BLACK);
  display.drawLine(x - 17 + lean, y + 9, x - 10 + lean, y + 15, GxEPD_BLACK);
  display.drawLine(x + 14 + lean, y + 6, x + 20 + lean, y + 11, GxEPD_BLACK);
  if (frame >= 1) {
    display.drawLine(x - 20 + lean, y - 3, x - 13 + lean, y + 1, GxEPD_BLACK);
    display.drawLine(x + 17 + lean, y - 4, x + 11 + lean, y + 1, GxEPD_BLACK);
    display.drawLine(x - 6 + lean, y + 18, x - 1 + lean, y + 11, GxEPD_BLACK);
    display.drawLine(x + 4 + lean, y + 15, x + 8 + lean, y + 20, GxEPD_BLACK);
  }
  if (frame >= 2) {
    display.drawLine(x - 18 + lean, y + 17, x - 12 + lean, y + 11, GxEPD_BLACK);
    display.drawLine(x + 15 + lean, y + 15, x + 20 + lean, y + 9, GxEPD_BLACK);
    display.fillTriangle(x - 3 + lean, y + 10, x + 1 + lean, y + 6, x + 4 + lean, y + 13, GxEPD_BLACK);
  }
}

void drawEggScaled(int x, int y, byte frame, int scalePercent) {
  int lean = frame == 1 ? -5 : frame == 2 ? 5 : 0;
  int scaledWidth = EGG_WIDTH * scalePercent / 100;
  int scaledHeight = EGG_HEIGHT * scalePercent / 100;
  drawScaledBitmap(x - scaledWidth / 2 + lean, y - scaledHeight / 2,
                   FLASH_ADDRESS(EGG_BITMAP), EGG_WIDTH, EGG_HEIGHT, scalePercent);
  if (frame >= 1) {
    display.drawLine(x - 22 + lean, y - 8, x - 12 + lean, y - 1, GxEPD_BLACK);
    display.drawLine(x - 12 + lean, y - 1, x - 19 + lean, y + 8, GxEPD_BLACK);
    display.drawLine(x + 19 + lean, y - 13, x + 10 + lean, y - 5, GxEPD_BLACK);
  }
  if (frame >= 2) {
    display.drawLine(x + 10 + lean, y - 5, x + 20 + lean, y + 5, GxEPD_BLACK);
    display.drawLine(x - 5 + lean, y + 15, x + 2 + lean, y + 5, GxEPD_BLACK);
  }
}

void drawRleBitmap(int x, int y, FlashAddress rle, uint16_t width, uint16_t height) {
  uint16_t px = 0;
  uint16_t py = 0;
  bool black = false;
  uint32_t index = 0;
  while (py < height) {
    uint8_t run = READ_FLASH_BYTE(rle + index++);
    if (run == 0) {
      black = !black;
      continue;
    }
    while (run > 0 && py < height) {
      uint16_t chunk = run < (width - px) ? run : (width - px);
      if (black) {
        display.fillRect(x + px, y + py, chunk, 1, GxEPD_BLACK);
      }
      px += chunk;
      run -= chunk;
      if (px >= width) {
        px = 0;
        py++;
      }
    }
    black = !black;
  }
}

void drawRleBitmapScaled(int x, int y, FlashAddress rle, uint16_t width, uint16_t height, int scalePercent) {
  uint16_t px = 0;
  uint16_t py = 0;
  bool black = false;
  uint32_t index = 0;
  while (py < height) {
    uint8_t run = READ_FLASH_BYTE(rle + index++);
    if (run == 0) {
      black = !black;
      continue;
    }
    while (run > 0 && py < height) {
      uint16_t chunk = run < (width - px) ? run : (width - px);
      if (black) {
        int outputX = x + px * scalePercent / 100;
        int outputY = y + py * scalePercent / 100;
        int outputRight = x + (px + chunk) * scalePercent / 100;
        int outputBottom = y + (py + 1) * scalePercent / 100;
        display.fillRect(outputX, outputY, outputRight - outputX, outputBottom - outputY, GxEPD_BLACK);
      }
      px += chunk;
      run -= chunk;
      if (px >= width) {
        px = 0;
        py++;
      }
    }
    black = !black;
  }
}

FlashAddress catActionFrame(Action action, byte frame) {
#define CAT_FRAME(NAME) \
  (frame == 0 ? FLASH_ADDRESS(CAT_##NAME##_0_RLE) : \
   frame == 1 ? FLASH_ADDRESS(CAT_##NAME##_1_RLE) : \
   frame == 2 ? FLASH_ADDRESS(CAT_##NAME##_2_RLE) : FLASH_ADDRESS(CAT_##NAME##_3_RLE))
  switch (action) {
    case FEED:
      return CAT_FRAME(FEED);
    case WATER:
      return CAT_FRAME(WATER);
    case SLEEP:
    case OVERNIGHT:
      return CAT_FRAME(SLEEP);
    case CLEAN:
      return CAT_FRAME(CLEAN);
    case MEDICINE:
      return CAT_FRAME(MEDICINE);
    case LEARN:
      return CAT_FRAME(LEARN);
    case PET_ACTION:
      return CAT_FRAME(PET);
    case GROOM:
      return CAT_FRAME(GROOM);
    case WASH:
      return CAT_FRAME(WASH);
    default:
      return FLASH_ADDRESS(CAT_FEED_0_RLE);
  }
#undef CAT_FRAME
}

FlashAddress speciesActionFrame(Animal kind, Action action, byte frame) {
#define ACTION_FRAME(PREFIX, NAME) \
  (frame == 0 ? FLASH_ADDRESS(PREFIX##_##NAME##_0_RLE) : \
   frame == 1 ? FLASH_ADDRESS(PREFIX##_##NAME##_1_RLE) : \
   frame == 2 ? FLASH_ADDRESS(PREFIX##_##NAME##_2_RLE) : FLASH_ADDRESS(PREFIX##_##NAME##_3_RLE))
#define ANIMAL_ACTION_FRAME(PREFIX) \
  ((action == SLEEP || action == OVERNIGHT) ? ACTION_FRAME(PREFIX, SLEEP) : \
   action == WATER ? ACTION_FRAME(PREFIX, WATER) : \
   action == CLEAN ? ACTION_FRAME(PREFIX, CLEAN) : \
   action == MEDICINE ? ACTION_FRAME(PREFIX, MEDICINE) : \
   action == LEARN ? ACTION_FRAME(PREFIX, LEARN) : \
   action == PET_ACTION ? ACTION_FRAME(PREFIX, PET) : \
   action == GROOM ? ACTION_FRAME(PREFIX, GROOM) : \
   action == WASH ? ACTION_FRAME(PREFIX, WASH) : ACTION_FRAME(PREFIX, FEED))
  switch (kind) {
    case DOG: return ANIMAL_ACTION_FRAME(DOG);
    case BUNNY: return ANIMAL_ACTION_FRAME(BUNNY);
    case PANDA: return ANIMAL_ACTION_FRAME(PANDA);
    case DRAGON: return ANIMAL_ACTION_FRAME(DRAGON);
    case FOX: return ANIMAL_ACTION_FRAME(FOX);
    case CHICKEN: return ANIMAL_ACTION_FRAME(CHICKEN);
    case PIG: return ANIMAL_ACTION_FRAME(PIG);
    case HAMSTER: return ANIMAL_ACTION_FRAME(HAMSTER);
    case PENGUIN: return ANIMAL_ACTION_FRAME(PENGUIN);
    default: return ANIMAL_ACTION_FRAME(DOG);
  }
#undef ANIMAL_ACTION_FRAME
#undef ACTION_FRAME
}

void animalBitmapInfo(Animal kind, FlashAddress &bitmap, byte &width, byte &height) {
  bitmap = FLASH_ADDRESS(CAT_BITMAP);
  width = CAT_WIDTH;
  height = CAT_HEIGHT;
  switch (kind) {
    case CAT: bitmap = FLASH_ADDRESS(CAT_BITMAP); width = CAT_WIDTH; height = CAT_HEIGHT; break;
    case DOG: bitmap = FLASH_ADDRESS(DOG_BITMAP); width = DOG_WIDTH; height = DOG_HEIGHT; break;
    case BUNNY: bitmap = FLASH_ADDRESS(BUNNY_BITMAP); width = BUNNY_WIDTH; height = BUNNY_HEIGHT; break;
    case PANDA: bitmap = FLASH_ADDRESS(PANDA_BITMAP); width = PANDA_WIDTH; height = PANDA_HEIGHT; break;
    case DRAGON: bitmap = FLASH_ADDRESS(DRAGON_BITMAP); width = DRAGON_WIDTH; height = DRAGON_HEIGHT; break;
    case FOX: bitmap = FLASH_ADDRESS(FOX_BITMAP); width = FOX_WIDTH; height = FOX_HEIGHT; break;
    case CHICKEN: bitmap = FLASH_ADDRESS(CHICKEN_BITMAP); width = CHICKEN_WIDTH; height = CHICKEN_HEIGHT; break;
    case PIG: bitmap = FLASH_ADDRESS(PIG_BITMAP); width = PIG_WIDTH; height = PIG_HEIGHT; break;
    case HAMSTER: bitmap = FLASH_ADDRESS(HAMSTER_BITMAP); width = HAMSTER_WIDTH; height = HAMSTER_HEIGHT; break;
    case PENGUIN: bitmap = FLASH_ADDRESS(PENGUIN_BITMAP); width = PENGUIN_WIDTH; height = PENGUIN_HEIGHT; break;
    default: break;
  }
}

void animalPoseBitmapInfo(Animal kind, AnimalPose pose, FlashAddress &bitmap, byte &width, byte &height) {
  animalBitmapInfo(kind, bitmap, width, height);
#define SET_ANIMAL_POSE(PREFIX) \
  bitmap = pose == POSE_HAPPY ? FLASH_ADDRESS(PREFIX##_HAPPY_BITMAP) : \
           pose == POSE_SLEEP ? FLASH_ADDRESS(PREFIX##_SLEEP_BITMAP) : FLASH_ADDRESS(PREFIX##_BITMAP)
  switch (kind) {
    case CAT: SET_ANIMAL_POSE(CAT); break;
    case DOG: SET_ANIMAL_POSE(DOG); break;
    case BUNNY: SET_ANIMAL_POSE(BUNNY); break;
    case PANDA: SET_ANIMAL_POSE(PANDA); break;
    case DRAGON: SET_ANIMAL_POSE(DRAGON); break;
    case FOX: SET_ANIMAL_POSE(FOX); break;
    case CHICKEN: SET_ANIMAL_POSE(CHICKEN); break;
    case PIG: SET_ANIMAL_POSE(PIG); break;
    case HAMSTER: SET_ANIMAL_POSE(HAMSTER); break;
    case PENGUIN: SET_ANIMAL_POSE(PENGUIN); break;
    default: break;
  }
#undef SET_ANIMAL_POSE
}

void drawAnimalScaled(int x, int y, Animal kind, byte pose, int scalePercent) {
  int bounce = pose % 2 ? -3 : 0;
  FlashAddress bitmap;
  byte width;
  byte height;
  animalBitmapInfo(kind, bitmap, width, height);
  int scaledWidth = width * scalePercent / 100;
  int scaledHeight = height * scalePercent / 100;
  drawScaledBitmap(x - scaledWidth / 2, y + bounce - scaledHeight / 2, bitmap, width, height, scalePercent);
}

void drawAnimal(int x, int y, Animal kind, byte pose) {
  int bounce = pose % 2 ? -3 : 0;
  int top = y + bounce;
  FlashAddress bitmap;
  byte width;
  byte height;
  animalPoseBitmapInfo(kind, pet.sleeping ? POSE_SLEEP : POSE_IDLE, bitmap, width, height);
  drawScaledBitmap(x - width / 2, top - height / 2, bitmap, width, height, 100);
  if (pet.sleeping) {
    display.setTextSize(1);
    display.setCursor(x + 25, top - 34);
    display.print(F("Z"));
    display.setCursor(x + 31, top - 28);
    display.print(F("Z"));
    display.setCursor(x + 37, top - 22);
    display.print(F("Z"));
  }
}

void drawActionIcon(Action action, int x, int y, bool selected) {
  if (selected) display.drawRoundRect(x - 2, y - 2, 32, 28, 6, GxEPD_BLACK);
  FlashAddress bitmap;
  switch (action) {
    case FEED: bitmap = FLASH_ADDRESS(ACTION_FOOD_ICON_BITMAP); break;
    case WATER: bitmap = FLASH_ADDRESS(ACTION_WATER_ICON_BITMAP); break;
    case PLAY: bitmap = FLASH_ADDRESS(ACTION_PLAY_ICON_BITMAP); break;
    case SLEEP: bitmap = FLASH_ADDRESS(ACTION_MOON_ICON_BITMAP); break;
    case OVERNIGHT: bitmap = FLASH_ADDRESS(ACTION_OVERNIGHT_ICON_BITMAP); break;
    case CLEAN: bitmap = FLASH_ADDRESS(ACTION_CLEAN_ICON_BITMAP); break;
    case MEDICINE: bitmap = FLASH_ADDRESS(ACTION_MEDICINE_ICON_BITMAP); break;
    case LEARN: bitmap = FLASH_ADDRESS(ACTION_LEARN_ICON_BITMAP); break;
    case PET_ACTION: bitmap = FLASH_ADDRESS(ACTION_PET_ICON_BITMAP); break;
    case GROOM: bitmap = FLASH_ADDRESS(ACTION_GROOM_ICON_BITMAP); break;
    case WASH: bitmap = FLASH_ADDRESS(ACTION_WASH_ICON_BITMAP); break;
    case SETTINGS: bitmap = FLASH_ADDRESS(ACTION_BED_ICON_BITMAP); break;
    default: return;
  }
  drawScaledBitmap(x, y, bitmap, ACTION_ICON_WIDTH, ACTION_ICON_HEIGHT, 100);
}

void printActionName(Action action) {
  switch (action) {
    case FEED: display.print(F("FOOD")); break;
    case WATER: display.print(F("WATER")); break;
    case PLAY: display.print(F("PLAY")); break;
    case SLEEP: display.print(F("NAP")); break;
    case OVERNIGHT: display.print(F("OVERNIGHT")); break;
    case CLEAN: display.print(F("CLEAN POOP")); break;
    case MEDICINE: display.print(F("MEDICINE")); break;
    case LEARN: display.print(F("READ")); break;
    case PET_ACTION: display.print(F("PET")); break;
    case GROOM: display.print(F("GROOM")); break;
    case WASH: display.print(F("BATH")); break;
    case SETTINGS: display.print(F("OPTIONS")); break;
    default: break;
  }
}

void drawHome() {
  drawMeter(5, 8, 90, 0, pet.food);
  drawMeter(5, 22, 90, 1, pet.water);
  drawMeter(105, 8, 90, 2, pet.happy);
  drawMeter(105, 22, 90, 3, pet.energy);
  drawAnimal(100, 88, animal, gameClock.minute);
  if (pet.poop) {
    display.fillCircle(164, 124, 7, GxEPD_BLACK);
    display.fillTriangle(157, 124, 164, 112, 171, 124, GxEPD_BLACK);
  }
  if (pet.sick) {
    drawVirus(28, 88);
  }
  if (pet.dirty) {
    display.drawLine(151, 80, 160, 72, GxEPD_BLACK);
    display.drawLine(161, 80, 170, 72, GxEPD_BLACK);
    display.drawLine(171, 80, 180, 72, GxEPD_BLACK);
  }
  for (byte i = 0; i < SETTINGS; i++) {
    int x = i < 6 ? 3 + i * 32 : 18 + (i - 6) * 36;
    int y = i < 6 ? 144 : 173;
    drawActionIcon((Action)i, x, y, selectedAction == i);
  }
}

void drawSetupNumber(const __FlashStringHelper *title, int value, const __FlashStringHelper *hint) {
  drawSetupFrame();
  drawCentered(title, 30, 2);
  drawBookDivider(54);
  display.drawRoundRect(55, 70, 90, 72, 14, GxEPD_BLACK);
  display.setTextSize(value > 99 ? 3 : 4);
  display.setCursor(value > 99 ? 64 : value < 10 ? 84 : 70, value > 99 ? 101 : 96);
  if (value < 10) display.print('0');
  display.print(value);
  drawSetupArrows(174);
}

void drawSetupScreen() {
  if (screen == SET_CLOCK) {
    drawSetupNumber(editField == 0 ? F("SET HOUR") : F("SET MINUTE"),
                    editField == 0 ? gameClock.hour : gameClock.minute,
                    F(""));
  } else {
    drawSetupFrame();
    drawAnimalScaled(100, 78, (Animal)animalChoice, 0, 130);
    display.fillRoundRect(58, 164, 84, 24, 10, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    drawCenteredInBox(animalName((Animal)animalChoice), 58, 164, 84, 24, 1);
    display.setTextColor(GxEPD_BLACK);
    display.fillTriangle(16, 100, 28, 92, 28, 108, GxEPD_BLACK);
    display.fillTriangle(184, 100, 172, 92, 172, 108, GxEPD_BLACK);
  }
}

void drawEggScreen() {
  drawEggScaled(100, 103, eggFrame, 130);
}

void drawScene() {
  drawClock();
  if (animal == CAT) {
    drawRleBitmapScaled(1, 42, catActionFrame(sceneAction, sceneFrame % 4),
                        CAT_ACTION_SCENE_WIDTH, CAT_ACTION_SCENE_HEIGHT, 108);
  } else {
    drawRleBitmap(8, 46, speciesActionFrame(animal, sceneAction, sceneFrame % 4),
                  SPECIES_ACTION_SCENE_WIDTH, SPECIES_ACTION_SCENE_HEIGHT);
  }
}

void drawGameMenu() {
  drawBookHeading(F("CHOOSE A GAME"), F("SELECT ONE"));
  display.drawRoundRect(18, 84, 164, 34, 8, GxEPD_BLACK);
  display.drawRoundRect(18, 128, 164, 34, 8, GxEPD_BLACK);
  display.setTextSize(2);
  display.setCursor(43, 96);
  display.print(F("GUESS SIDE"));
  display.setCursor(55, 140);
  display.print(F("STOP BAR"));
  display.drawRect(21, gameChoice == 0 ? 87 : 131, 8, 28, GxEPD_BLACK);
  display.setTextSize(1);
  display.setCursor(36, 181);
  display.print(F("< choose   SELECT play   choose >"));
}

void drawOptions() {
  drawBookHeading(F("OPTIONS"), F("SELECT WHAT TO CHANGE"));
  display.setTextSize(2);
  display.setCursor(34, 96);
  display.print(editField == 0 ? F("> SET CLOCK") : F("  SET CLOCK"));
  display.setCursor(34, 140);
  display.print(editField == 1 ? F("> NEW EGG") : F("  NEW EGG"));
  display.setTextSize(1);
  display.setCursor(40, 181);
  display.print(F("< move   SELECT   move >"));
}

void refreshDisplay() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    if (screen == LANGUAGE) drawLanguageScreen();
    else if (screen == SET_CLOCK || screen == SELECT_ANIMAL) drawSetupScreen();
    else if (screen == EGG) drawEggScreen();
    else if (screen == HOME) drawHome();
    else if (screen == ACTION_SCENE) drawScene();
    else if (screen == GAME_MENU) drawGameMenu();
    else if (screen == OPTIONS) drawOptions();
  } while (display.nextPage());
  display.hibernate();
  displayDirty = false;
}

void animateAction(Action action) {
  sceneAction = action;
  screen = ACTION_SCENE;
  for (sceneFrame = 0; sceneFrame < 4; sceneFrame++) {
    refreshDisplay();
    chirp(650 + sceneFrame * 180, 70);
    delay(160);
  }
  sceneFrame = 3;
  delay(1000);
  screen = HOME;
  displayDirty = true;
  refreshDisplay();
}

void animateEggHatch() {
  screen = HATCHING;
  for (byte frame = 0; frame < 4; frame++) {
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      if (frame < 3) {
        drawEggScaled(100, 103, frame, 130);
      } else {
        drawAnimalScaled(100, 96, animal, 1, 130);
        drawHeart(24, 54);
        drawHeart(164, 62);
      }
    } while (display.nextPage());
    chirp(500 + frame * 180, 160);
    delay(350);
  }
  hatchTune();
  screen = HOME;
  saveGame(HOME);
  displayDirty = true;
}

void startEgg() {
  pet = {80, 80, 80, 80, 100, 0, 0, false, false, false, 0};
  hatchMinutesLeft = random(120, 301);
  screen = EGG;
  eggFrame = 0;
  saveGame(EGG);
  happyTune();
  displayDirty = true;
}

void advanceClock() {
  gameClock.minute++;
  if (gameClock.minute < 60) return;
  gameClock.minute = 0;
  gameClock.hour++;
  if (gameClock.hour < 24) return;
  gameClock.hour = 0;
  gameClock.day++;
  pet.ageDays++;
  if (gameClock.day > daysInMonth(gameClock.month, gameClock.year)) {
    gameClock.day = 1;
    gameClock.month++;
    if (gameClock.month > 12) {
      gameClock.month = 1;
      gameClock.year++;
    }
  }
}

void updateNeeds() {
  if (screen != HOME && screen != ACTION_SCENE && screen != GAME_MENU) return;
  if (pet.sleeping) {
    pet.energy = clampStat(pet.energy + 8);
    pet.food = clampStat(pet.food - 2);
    pet.water = clampStat(pet.water - 2);
  } else {
    pet.food = clampStat(pet.food - 4);
    pet.water = clampStat(pet.water - 5);
    pet.happy = clampStat(pet.happy - 3);
    pet.energy = clampStat(pet.energy - 3);
  }
  if (random(0, 5) == 0 && pet.poop < 3) pet.poop++;
  if (random(0, 8) == 0) pet.dirty = true;
  if ((pet.poop >= 2 || pet.dirty) && random(0, 5) == 0) pet.sick = true;
  if (pet.sick) pet.health = clampStat(pet.health - 5);
  saveGame(HOME);
  displayDirty = true;
}

void showMessage(const __FlashStringHelper *title, const __FlashStringHelper *message) {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawCentered(title, 55, 2);
    drawCentered(message, 100);
    drawCentered(F("Press SELECT"), 160);
  } while (display.nextPage());
  while (!pressed(selectButton)) delay(10);
}

void runGuessGame() {
  byte answer = random(0, 2);
  showMessage(F("GUESS SIDE"), F("LEFT or RIGHT?"));
  byte guess = 2;
  while (guess == 2) {
    if (pressed(leftButton)) guess = 0;
    if (pressed(rightButton)) guess = 1;
  }
  bool won = guess == answer;
  pet.happy = clampStat(pet.happy + (won ? 20 : 5));
  pet.energy = clampStat(pet.energy - 8);
  showMessage(won ? F("YOU WON!") : F("NICE TRY"), won ? F("+20 happiness") : F("+5 happiness"));
  if (won) happyTune(); else chirp(300, 160);
}

void runStopGame() {
  byte marker = 0;
  bool direction = true;
  while (!pressed(selectButton)) {
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      drawCentered(F("STOP IN THE MIDDLE"), 40, 2);
      display.drawRect(20, 100, 160, 20, GxEPD_BLACK);
      display.fillRect(92, 102, 16, 16, GxEPD_BLACK);
      display.fillRect(22 + marker, 96, 4, 28, GxEPD_BLACK);
    } while (display.nextPage());
    marker += direction ? 16 : -16;
    if (marker >= 144) direction = false;
    if (marker == 0) direction = true;
  }
  bool won = marker >= 64 && marker <= 96;
  pet.happy = clampStat(pet.happy + (won ? 20 : 5));
  pet.energy = clampStat(pet.energy - 8);
  showMessage(won ? F("PERFECT!") : F("NICE TRY"), won ? F("+20 happiness") : F("+5 happiness"));
  if (won) happyTune(); else chirp(300, 160);
}

void performAction(Action action) {
  if (action == PLAY) {
    gameChoice = 0;
    screen = GAME_MENU;
    displayDirty = true;
    return;
  }
  if (action == SETTINGS) {
    editField = 0;
    screen = OPTIONS;
    displayDirty = true;
    return;
  }

  sceneAction = action;
  screen = ACTION_SCENE;
  switch (action) {
    case FEED: pet.food = clampStat(pet.food + 25); break;
    case WATER: pet.water = clampStat(pet.water + 30); break;
    case SLEEP: pet.sleeping = !pet.sleeping; pet.energy = clampStat(pet.energy + 8); break;
    case OVERNIGHT: pet.sleeping = true; pet.energy = 100; pet.food = clampStat(pet.food - 8); break;
    case CLEAN: pet.poop = 0; pet.happy = clampStat(pet.happy + 5); break;
    case MEDICINE: pet.sick = false; pet.health = clampStat(pet.health + 35); break;
    case LEARN: pet.learning = clampStat(pet.learning + 15); pet.happy = clampStat(pet.happy + 5); break;
    case PET_ACTION: pet.happy = clampStat(pet.happy + 15); break;
    case GROOM: pet.dirty = false; pet.happy = clampStat(pet.happy + 10); break;
    case WASH: pet.dirty = false; pet.health = clampStat(pet.health + 5); break;
    default: break;
  }
  saveGame(HOME);
  animateAction(action);
  happyTune();
}

void changeSetupValue(int direction) {
  if (screen == LANGUAGE) {
    languageChoice = (languageChoice + 3 + direction) % 3;
  } else if (screen == SET_CLOCK) {
    if (editField == 0) gameClock.hour = (gameClock.hour + 24 + direction) % 24;
    else gameClock.minute = (gameClock.minute + 60 + direction) % 60;
  } else if (screen == SELECT_ANIMAL) {
    animalChoice = (animalChoice + ANIMAL_COUNT + direction) % ANIMAL_COUNT;
  }
  displayDirty = true;
}

void handleButtons(bool left, bool select, bool right) {
  if (screen == LANGUAGE) {
    if (left) changeSetupValue(-1);
    if (right) changeSetupValue(1);
    if (select) {
      screen = SET_CLOCK;
      editField = 0;
      setupCreatesEgg = true;
      displayDirty = true;
    }
    return;
  }
  if (screen == SET_CLOCK || screen == SELECT_ANIMAL) {
    if (left) changeSetupValue(-1);
    if (right) changeSetupValue(1);
    if (select) {
      editField++;
      if (screen == SET_CLOCK && editField > 1) {
        screen = setupCreatesEgg ? SELECT_ANIMAL : HOME;
        editField = 0;
        if (!setupCreatesEgg) saveGame(HOME);
      }
      else if (screen == SELECT_ANIMAL) { animal = (Animal)animalChoice; startEgg(); }
      displayDirty = true;
    }
    return;
  }
  if (screen == EGG) {
    if (select) {
      unsigned long now = millis();
      if (now - lastEggSelect > 700) eggSelectCount = 0;
      lastEggSelect = now;
      eggSelectCount++;
      if (eggSelectCount >= 3) {
        eggSelectCount = 0;
        hatchMinutesLeft = 0;
        animateEggHatch();
        return;
      }
    }
    return;
  }
  if (screen == HOME) {
    if (left) selectedAction = (Action)((selectedAction + SETTINGS - 1) % SETTINGS);
    if (right) selectedAction = (Action)((selectedAction + 1) % SETTINGS);
    if (select) performAction(selectedAction);
    if (left || right) { chirp(900, 25); displayDirty = true; }
  } else if (screen == ACTION_SCENE && select) {
    screen = HOME;
    displayDirty = true;
  } else if (screen == GAME_MENU) {
    if (left || right) { gameChoice = !gameChoice; displayDirty = true; }
    if (select) {
      if (gameChoice == 0) runGuessGame(); else runStopGame();
      saveGame(HOME);
      screen = HOME;
      displayDirty = true;
    }
  } else if (screen == OPTIONS) {
    if (left || right) {
      editField = !editField;
      displayDirty = true;
    }
    if (select) {
      setupCreatesEgg = editField == 1;
      if (editField == 0) { screen = SET_CLOCK; editField = 0; }
      else { screen = SELECT_ANIMAL; animalChoice = animal; }
      displayDirty = true;
    }
  }
}

void setup() {
  pinMode(LEFT_PIN, INPUT_PULLUP);
  pinMode(SELECT_PIN, INPUT_PULLUP);
  pinMode(RIGHT_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  Serial.begin(9600);
#ifdef WOKWI_SIM
  Serial.println(F("Tamagotchi simulator boot"));
#endif
  randomSeed(analogRead(A0));
#if defined(ESP32)
  EEPROM.begin(512);
#endif
  display.init(115200);
  display.setRotation(0);
  if (!loadGame()) {
    screen = LANGUAGE;
    editField = 0;
    setupCreatesEgg = true;
  }
  lastClockTick = millis();
  lastNeedsTick = millis();
  lastEggFrame = millis();
  startupShortcutArmed = true;
  startupSelectHeldSince = 0;
  eggSelectCount = 0;
  refreshDisplay();
}

void loop() {
  if (startupShortcutArmed && (screen == LANGUAGE || screen == SET_CLOCK)) {
    if (digitalRead(SELECT_PIN) == LOW) {
      if (startupSelectHeldSince == 0) startupSelectHeldSince = millis();
      if (millis() - startupSelectHeldSince >= 1000UL) {
        startupShortcutArmed = false;
        startupSelectHeldSince = 0;
        animalChoice = animal;
        editField = 0;
        screen = SELECT_ANIMAL;
        displayDirty = true;
        refreshDisplay();
        return;
      }
      return;
    } else if (startupSelectHeldSince != 0) {
      startupShortcutArmed = false;
      startupSelectHeldSince = 0;
      handleButtons(false, true, false);
      return;
    }
  }

  bool left = pressed(leftButton);
  bool select = pressed(selectButton);
  bool right = pressed(rightButton);
  if (left || select || right) handleButtons(left, select, right);

  unsigned long now = millis();
  while (now - lastClockTick >= CLOCK_TICK_MS) {
    lastClockTick += CLOCK_TICK_MS;
    advanceClock();
    if (screen == EGG && hatchMinutesLeft > 0) hatchMinutesLeft--;
    if (screen == EGG && hatchMinutesLeft == 0) animateEggHatch();
    if (gameClock.minute == 0) saveGame(screen == EGG ? EGG : HOME);
    displayDirty = true;
  }
  if (now - lastNeedsTick >= NEEDS_TICK_MS) {
    lastNeedsTick = now;
    updateNeeds();
  }
  if (screen == EGG && now - lastEggFrame >= EGG_FRAME_MS) {
    lastEggFrame = now;
    eggFrame = (eggFrame + 1) % 3;
    displayDirty = true;
  }
  if (displayDirty) refreshDisplay();
}
