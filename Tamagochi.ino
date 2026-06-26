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
#include <U8g2_for_Adafruit_GFX.h>
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
const unsigned int PET_ADULT_DAYS = 30;
const byte WORK_SHORTCUT_PRESSES = 5;

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
U8G2_FOR_ADAFRUIT_GFX u8g2Text;

enum Screen : byte {
  LANGUAGE = 0, SET_CLOCK = 1, SELECT_ANIMAL = 3, EGG = 4, HATCHING = 5,
  HOME = 6, ACTION_SCENE = 7, GAME_MENU = 8, OPTIONS = 9, GAME_PLAY = 10,
  GROWN_UP = 11
};
enum Animal : byte { CAT, DOG, BUNNY, PANDA, DRAGON, FOX, CHICKEN, PIG, HAMSTER, PENGUIN, ANIMAL_COUNT };
enum AnimalPose : byte { POSE_IDLE, POSE_HAPPY, POSE_SLEEP };
enum Action : byte {
  FEED, WATER, PLAY, SLEEP, OVERNIGHT, CLEAN, MEDICINE,
  LEARN, PET_ACTION, GROOM, WASH, SETTINGS, ACTION_COUNT
};
enum MiniGame : byte { GAME_HIGH_LOW, GAME_COIN_TOSS, GAME_SHELL, MINI_GAME_COUNT };
enum MiniGamePhase : byte { MINI_GAME_PICK, MINI_GAME_RESULT };
enum UiText : byte {
  TXT_SET_HOUR, TXT_SET_MINUTE, TXT_OPTIONS, TXT_CHANGE, TXT_SET_CLOCK, TXT_NEW_EGG,
  TXT_OPTIONS_HINT, TXT_FOOD, TXT_WATER, TXT_PLAY, TXT_NAP, TXT_OVERNIGHT,
  TXT_CLEAN, TXT_MEDICINE, TXT_READ, TXT_PET, TXT_GROOM, TXT_BATH,
  TXT_GAME_HIGH_LOW_MENU, TXT_GAME_HIGH_LOW_TITLE, TXT_GAME_COIN, TXT_GAME_SHELL,
  TXT_HIGHER, TXT_LOWER, TXT_NEXT, TXT_CORRECT_20, TXT_WRONG_5,
  TXT_HEADS, TXT_TAILS, TXT_COIN_HEAD_MARK, TXT_COIN_TAIL_MARK,
  TXT_YOU_GOT_IT, TXT_MISSED, TXT_PLUS_20_HAPPY, TXT_PLUS_5_HAPPY,
  TXT_FIND_BALL, TXT_MOVE, TXT_FOUND_20, TXT_EMPTY_5,
  TXT_GROWN_TITLE, TXT_GROWN_DAY, TXT_GROWN_WORK
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
  byte language;
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
MiniGame activeGame = GAME_HIGH_LOW;
MiniGamePhase miniGamePhase = MINI_GAME_PICK;
byte gameCurrentNumber = 0;
byte gameNextNumber = 0;
byte coinAnswer = 0;
byte shellPick = 1;
byte shellAnswer = 0;
bool miniGameWon = false;
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
byte workShortcutRightCount = 0;
byte workShortcutLeftCount = 0;

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
  (void)frequency;
  (void)duration;
}

void playTune(const int *notes, const uint16_t *lengths, byte count) {
  (void)notes;
  (void)lengths;
  (void)count;
}

void happyTune() {
}

void hatchTune() {
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

bool savedStageValid(byte stage) {
  return stage == EGG || stage == HOME || stage == GROWN_UP;
}

byte currentSaveStage() {
  if (screen == EGG) return EGG;
  if (screen == GROWN_UP) return GROWN_UP;
  return HOME;
}

void saveGame(byte stage) {
  SaveData data = {SAVE_MAGIC, gameClock, pet, (byte)animal, stage, hatchMinutesLeft, languageChoice};
  EEPROM.put(0, data);
#if defined(ESP32)
  EEPROM.commit();
#endif
}

bool loadGame() {
  SaveData data;
  EEPROM.get(0, data);
  if (data.magic != SAVE_MAGIC || data.animal >= ANIMAL_COUNT || !savedStageValid(data.stage)) {
    return false;
  }
  gameClock = data.clock;
  pet = data.pet;
  animal = (Animal)data.animal;
  languageChoice = data.language < 3 ? data.language : 0;
  hatchMinutesLeft = data.hatchMinutesLeft;
  if (data.stage == EGG) screen = EGG;
  else if (data.stage == GROWN_UP || pet.ageDays >= PET_ADULT_DAYS) screen = GROWN_UP;
  else screen = HOME;
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

const __FlashStringHelper *uiText(UiText text) {
  if (languageChoice == 1) {
    return F("");
  } else if (languageChoice == 2) {
    switch (text) {
      case TXT_SET_HOUR: return F("STUNDE");
      case TXT_SET_MINUTE: return F("MINUTE");
      case TXT_OPTIONS: return F("OPTION");
      case TXT_CHANGE: return F("AENDERN");
      case TXT_SET_CLOCK: return F("UHR STELLEN");
      case TXT_NEW_EGG: return F("NEUES EI");
      case TXT_OPTIONS_HINT: return F("< > WAHL   OK");
      case TXT_FOOD: return F("FUTTER");
      case TXT_WATER: return F("WASSER");
      case TXT_PLAY: return F("SPIEL");
      case TXT_NAP: return F("SCHLAF");
      case TXT_OVERNIGHT: return F("NACHT");
      case TXT_CLEAN: return F("SAUBER");
      case TXT_MEDICINE: return F("MEDIZIN");
      case TXT_READ: return F("LESEN");
      case TXT_PET: return F("STREICH");
      case TXT_GROOM: return F("KAMM");
      case TXT_BATH: return F("BAD");
      case TXT_GAME_HIGH_LOW_MENU: return F("HOCH / TIEF");
      case TXT_GAME_HIGH_LOW_TITLE: return F("HOCH TIEF");
      case TXT_GAME_COIN: return F("MUENZWURF");
      case TXT_GAME_SHELL: return F("BECHER");
      case TXT_HIGHER: return F("HOCH");
      case TXT_LOWER: return F("TIEF");
      case TXT_NEXT: return F("NAECH");
      case TXT_CORRECT_20: return F("RICHTIG +20");
      case TXT_WRONG_5: return F("FALSCH +5");
      case TXT_HEADS: return F("KOPF");
      case TXT_TAILS: return F("ZAHL");
      case TXT_COIN_HEAD_MARK: return F("K");
      case TXT_COIN_TAIL_MARK: return F("Z");
      case TXT_YOU_GOT_IT: return F("RICHTIG");
      case TXT_MISSED: return F("DANEBEN");
      case TXT_PLUS_20_HAPPY: return F("+20 GLUECK");
      case TXT_PLUS_5_HAPPY: return F("+5 GLUECK");
      case TXT_FIND_BALL: return F("FINDE BALL");
      case TXT_MOVE: return F("WECHSEL");
      case TXT_FOUND_20: return F("GEFUNDEN +20");
      case TXT_EMPTY_5: return F("LEER +5");
      case TXT_GROWN_TITLE: return F("GROSS!");
      case TXT_GROWN_DAY: return F("TAG 30");
      case TXT_GROWN_WORK: return F("ZUR ARBEIT");
    }
  }

  switch (text) {
    case TXT_SET_HOUR: return F("SET HOUR");
    case TXT_SET_MINUTE: return F("SET MINUTE");
    case TXT_OPTIONS: return F("OPTIONS");
    case TXT_CHANGE: return F("CHANGE");
    case TXT_SET_CLOCK: return F("SET CLOCK");
    case TXT_NEW_EGG: return F("NEW EGG");
    case TXT_OPTIONS_HINT: return F("< > MOVE   OK");
    case TXT_FOOD: return F("FOOD");
    case TXT_WATER: return F("WATER");
    case TXT_PLAY: return F("PLAY");
    case TXT_NAP: return F("NAP");
    case TXT_OVERNIGHT: return F("NIGHT");
    case TXT_CLEAN: return F("CLEAN");
    case TXT_MEDICINE: return F("MEDS");
    case TXT_READ: return F("READ");
    case TXT_PET: return F("PET");
    case TXT_GROOM: return F("GROOM");
    case TXT_BATH: return F("BATH");
    case TXT_GAME_HIGH_LOW_MENU: return F("HIGHER / LOWER");
    case TXT_GAME_HIGH_LOW_TITLE: return F("HIGH LOW");
    case TXT_GAME_COIN: return F("COIN TOSS");
    case TXT_GAME_SHELL: return F("SHELL GAME");
    case TXT_HIGHER: return F("HIGHER");
    case TXT_LOWER: return F("LOWER");
    case TXT_NEXT: return F("NEXT");
    case TXT_CORRECT_20: return F("CORRECT +20");
    case TXT_WRONG_5: return F("WRONG +5");
    case TXT_HEADS: return F("HEADS");
    case TXT_TAILS: return F("TAILS");
    case TXT_COIN_HEAD_MARK: return F("H");
    case TXT_COIN_TAIL_MARK: return F("T");
    case TXT_YOU_GOT_IT: return F("YOU GOT IT");
    case TXT_MISSED: return F("MISSED");
    case TXT_PLUS_20_HAPPY: return F("+20 HAPPY");
    case TXT_PLUS_5_HAPPY: return F("+5 HAPPY");
    case TXT_FIND_BALL: return F("FIND THE BALL");
    case TXT_MOVE: return F("MOVE");
    case TXT_FOUND_20: return F("FOUND +20");
    case TXT_EMPTY_5: return F("EMPTY +5");
    case TXT_GROWN_TITLE: return F("GROWN UP!");
    case TXT_GROWN_DAY: return F("DAY 30");
    case TXT_GROWN_WORK: return F("OFF TO WORK");
  }
  return F("");
}

const char *bgText(UiText text) {
  switch (text) {
    case TXT_SET_HOUR: return "ЧАС";
    case TXT_SET_MINUTE: return "МИНУТА";
    case TXT_OPTIONS: return "ОПЦИИ";
    case TXT_CHANGE: return "ПРОМЕНИ";
    case TXT_SET_CLOCK: return "ЧАСОВНИК";
    case TXT_NEW_EGG: return "НОВО ЯЙЦЕ";
    case TXT_OPTIONS_HINT: return "< > МЕСТИ   OK";
    case TXT_FOOD: return "ХРАНА";
    case TXT_WATER: return "ВОДА";
    case TXT_PLAY: return "ИГРА";
    case TXT_NAP: return "СЪН";
    case TXT_OVERNIGHT: return "НОЩ";
    case TXT_CLEAN: return "ЧИСТИ";
    case TXT_MEDICINE: return "ЛЕК";
    case TXT_READ: return "ЧЕТИ";
    case TXT_PET: return "ГАЛИ";
    case TXT_GROOM: return "РЕШИ";
    case TXT_BATH: return "БАНЯ";
    case TXT_GAME_HIGH_LOW_MENU: return "ГОЛЯМО / МАЛКО";
    case TXT_GAME_HIGH_LOW_TITLE: return "ГОЛЯМО МАЛКО";
    case TXT_GAME_COIN: return "МОНЕТА";
    case TXT_GAME_SHELL: return "ЧАШКИ";
    case TXT_HIGHER: return "ГОЛЯМО";
    case TXT_LOWER: return "МАЛКО";
    case TXT_NEXT: return "СЛЕД";
    case TXT_CORRECT_20: return "ВЯРНО +20";
    case TXT_WRONG_5: return "ГРЕШНО +5";
    case TXT_HEADS: return "ЕЗИ";
    case TXT_TAILS: return "ТУРА";
    case TXT_COIN_HEAD_MARK: return "Е";
    case TXT_COIN_TAIL_MARK: return "Т";
    case TXT_YOU_GOT_IT: return "ПОЗНА";
    case TXT_MISSED: return "НЕ";
    case TXT_PLUS_20_HAPPY: return "+20 РАДОСТ";
    case TXT_PLUS_5_HAPPY: return "+5 РАДОСТ";
    case TXT_FIND_BALL: return "НАМЕРИ ТОПКА";
    case TXT_MOVE: return "МЕСТИ";
    case TXT_FOUND_20: return "НАМЕРИ +20";
    case TXT_EMPTY_5: return "ПРАЗНО +5";
    case TXT_GROWN_TITLE: return "ПОРАСНА!";
    case TXT_GROWN_DAY: return "ДЕН 30";
    case TXT_GROWN_WORK: return "НА РАБОТА";
  }
  return "";
}

UiText actionLabel(Action action) {
  switch (action) {
    case FEED: return TXT_FOOD;
    case WATER: return TXT_WATER;
    case PLAY: return TXT_PLAY;
    case SLEEP: return TXT_NAP;
    case OVERNIGHT: return TXT_OVERNIGHT;
    case CLEAN: return TXT_CLEAN;
    case MEDICINE: return TXT_MEDICINE;
    case LEARN: return TXT_READ;
    case PET_ACTION: return TXT_PET;
    case GROOM: return TXT_GROOM;
    case WASH: return TXT_BATH;
    case SETTINGS: return TXT_OPTIONS;
    default: return TXT_OPTIONS;
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

bool useCyrillicText() {
  return languageChoice == 1;
}

void setCyrillicFont(byte size, uint16_t color) {
  u8g2Text.setFont(size >= 2 ? u8g2_font_9x15_t_cyrillic : u8g2_font_6x12_t_cyrillic);
  u8g2Text.setFontMode(1);
  u8g2Text.setFontDirection(0);
  u8g2Text.setForegroundColor(color);
}

int uiTextWidth(UiText text, byte size = 1) {
  if (useCyrillicText()) {
    setCyrillicFont(size, GxEPD_BLACK);
    return u8g2Text.getUTF8Width(bgText(text));
  }
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(uiText(text), 0, 0, &x1, &y1, &w, &h);
  return w;
}

int uiTextHeight(byte size = 1) {
  if (useCyrillicText()) {
    setCyrillicFont(size, GxEPD_BLACK);
    return u8g2Text.getFontAscent() - u8g2Text.getFontDescent();
  }
  return 8 * size;
}

void drawUiText(UiText text, int x, int y, byte size = 1, uint16_t color = GxEPD_BLACK) {
  if (useCyrillicText()) {
    setCyrillicFont(size, color);
    u8g2Text.drawUTF8(x, y + u8g2Text.getFontAscent(), bgText(text));
  } else {
    display.setTextColor(color);
    display.setTextSize(size);
    display.setCursor(x, y);
    display.print(uiText(text));
  }
}

void drawUiCentered(UiText text, int y, byte size = 1, uint16_t color = GxEPD_BLACK) {
  drawUiText(text, (200 - uiTextWidth(text, size)) / 2, y, size, color);
}

void drawUiCenteredInBox(UiText text, int x, int y, int w, int h, byte size = 1,
                         uint16_t color = GxEPD_BLACK) {
  int tw = uiTextWidth(text, size);
  int th = uiTextHeight(size);
  drawUiText(text, x + (w - tw) / 2, y + (h - th) / 2, size, color);
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

void drawBookHeading(UiText title, UiText subtitle) {
  drawBookFrame();
  drawUiCentered(title, 58, 2);
  drawBookDivider(68);
  drawUiCentered(subtitle, 90, 1);
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
  int lean = 0;
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
  int lean = 0;
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

int animalDisplayScale(Animal kind) {
  return 100;
}

void drawAnimalScaled(int x, int y, Animal kind, byte pose, int scalePercent) {
  int bounce = pose % 2 ? -3 : 0;
  FlashAddress bitmap;
  byte width;
  byte height;
  animalBitmapInfo(kind, bitmap, width, height);
  int effectiveScale = scalePercent * animalDisplayScale(kind) / 100;
  int scaledWidth = width * effectiveScale / 100;
  int scaledHeight = height * effectiveScale / 100;
  drawScaledBitmap(x - scaledWidth / 2, y + bounce - scaledHeight / 2, bitmap, width, height, effectiveScale);
}

void drawAnimal(int x, int y, Animal kind, byte pose) {
  int bounce = pose % 2 ? -3 : 0;
  int top = y + bounce;
  FlashAddress bitmap;
  byte width;
  byte height;
  animalPoseBitmapInfo(kind, pet.sleeping ? POSE_SLEEP : POSE_IDLE, bitmap, width, height);
  int scalePercent = animalDisplayScale(kind);
  int scaledWidth = width * scalePercent / 100;
  int scaledHeight = height * scalePercent / 100;
  drawScaledBitmap(x - scaledWidth / 2, top - scaledHeight / 2, bitmap, width, height, scalePercent);
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
  if (useCyrillicText()) {
    setCyrillicFont(1, GxEPD_BLACK);
    u8g2Text.print(bgText(actionLabel(action)));
  } else {
    display.print(uiText(actionLabel(action)));
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

void drawBriefcase(int x, int y) {
  display.drawRoundRect(x, y, 38, 27, 4, GxEPD_BLACK);
  display.drawRoundRect(x + 12, y - 6, 14, 8, 3, GxEPD_BLACK);
  display.drawLine(x, y + 11, x + 38, y + 11, GxEPD_BLACK);
  display.fillCircle(x + 19, y + 11, 2, GxEPD_BLACK);
}

void drawGrownUpScreen() {
  display.drawRoundRect(6, 6, 188, 188, 12, GxEPD_BLACK);
  display.drawRoundRect(10, 10, 180, 180, 10, GxEPD_BLACK);
  drawUiCentered(TXT_GROWN_TITLE, 18, 2);
  drawUiCentered(TXT_GROWN_DAY, 43, 1);
  display.drawLine(38, 58, 162, 58, GxEPD_BLACK);
  drawAnimalScaled(82, 112, animal, 1, 78);
  drawBriefcase(126, 123);
  display.drawLine(36, 158, 164, 158, GxEPD_BLACK);
  display.fillRect(47, 155, 14, 4, GxEPD_BLACK);
  display.fillRect(139, 155, 14, 4, GxEPD_BLACK);
  drawUiCentered(TXT_GROWN_WORK, 169, 1);
}

void drawSetupNumber(UiText title, int value) {
  drawSetupFrame();
  drawUiCentered(title, 30, 2);
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
    drawSetupNumber(editField == 0 ? TXT_SET_HOUR : TXT_SET_MINUTE,
                    editField == 0 ? gameClock.hour : gameClock.minute);
  } else {
    drawSetupFrame();
    drawAnimalScaled(100, 78, (Animal)animalChoice, 0, 100);
    display.fillRoundRect(58, 164, 84, 24, 10, GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    drawCenteredInBox(animalName((Animal)animalChoice), 58, 164, 84, 24, 1);
    display.setTextColor(GxEPD_BLACK);
    display.fillTriangle(16, 100, 28, 92, 28, 108, GxEPD_BLACK);
    display.fillTriangle(184, 100, 172, 92, 172, 108, GxEPD_BLACK);
  }
}

void drawEggScreen() {
  drawEggScaled(100, 100, eggFrame, 100);
}

void drawScene() {
  if (animal == CAT) {
    drawRleBitmap((200 - CAT_ACTION_SCENE_WIDTH) / 2,
                  (200 - CAT_ACTION_SCENE_HEIGHT) / 2,
                  catActionFrame(sceneAction, sceneFrame % 4),
                  CAT_ACTION_SCENE_WIDTH, CAT_ACTION_SCENE_HEIGHT);
  } else {
    drawRleBitmap((200 - SPECIES_ACTION_SCENE_WIDTH) / 2,
                  (200 - SPECIES_ACTION_SCENE_HEIGHT) / 2,
                  speciesActionFrame(animal, sceneAction, sceneFrame % 4),
                  SPECIES_ACTION_SCENE_WIDTH, SPECIES_ACTION_SCENE_HEIGHT);
  }
}

void drawMenuRow(bool selected, int y, UiText label) {
  if (selected) {
    display.fillRoundRect(17, y, 166, 28, 8, GxEPD_BLACK);
    drawUiCenteredInBox(label, 17, y, 166, 28, 1, GxEPD_WHITE);
  } else {
    display.drawRoundRect(17, y, 166, 28, 8, GxEPD_BLACK);
    drawUiCenteredInBox(label, 17, y, 166, 28, 1, GxEPD_BLACK);
  }
  display.setTextColor(GxEPD_BLACK);
}

void drawGameMenuRow(byte index, int y, UiText label) {
  drawMenuRow(gameChoice == index, y, label);
}

void drawOptionRow(byte index, int y, UiText label) {
  drawMenuRow(editField == index, y, label);
}

void drawGameMenu() {
  display.drawRoundRect(6, 6, 188, 188, 12, GxEPD_BLACK);
  display.drawRoundRect(10, 10, 180, 180, 10, GxEPD_BLACK);
  display.setTextColor(GxEPD_BLACK);
  drawGameMenuRow(0, 38, TXT_GAME_HIGH_LOW_MENU);
  drawGameMenuRow(1, 86, TXT_GAME_COIN);
  drawGameMenuRow(2, 134, TXT_GAME_SHELL);
}

void drawMiniGameFrame(UiText title) {
  display.drawRoundRect(6, 6, 188, 188, 12, GxEPD_BLACK);
  display.drawRoundRect(10, 10, 180, 180, 10, GxEPD_BLACK);
  display.setTextColor(GxEPD_BLACK);
  drawUiCenteredInBox(title, 18, 18, 164, 28, 2);
  display.drawLine(29, 50, 171, 50, GxEPD_BLACK);
  display.drawCircle(36, 43, 3, GxEPD_BLACK);
  display.drawCircle(164, 43, 3, GxEPD_BLACK);
}

void drawChoiceButton(int x, int y, int w, UiText label) {
  display.drawRoundRect(x, y, w, 24, 7, GxEPD_BLACK);
  drawUiCenteredInBox(label, x, y, w, 24, 1);
}

void drawHigherLowerGame() {
  drawMiniGameFrame(TXT_GAME_HIGH_LOW_TITLE);
  display.drawRoundRect(61, 61, 78, 80, 10, GxEPD_BLACK);
  display.drawRoundRect(68, 68, 64, 66, 7, GxEPD_BLACK);
  display.setTextSize(5);
  int x = gameCurrentNumber < 10 ? 86 : 72;
  display.setCursor(x, 87);
  display.print(gameCurrentNumber);
  display.setTextSize(1);
  if (miniGamePhase == MINI_GAME_PICK) {
    display.fillTriangle(54, 154, 43, 143, 65, 143, GxEPD_BLACK);
    display.fillTriangle(146, 143, 135, 154, 157, 154, GxEPD_BLACK);
    drawChoiceButton(25, 160, 70, TXT_LOWER);
    drawChoiceButton(105, 160, 70, TXT_HIGHER);
  } else {
    display.drawRoundRect(65, 139, 70, 24, 6, GxEPD_BLACK);
    drawUiText(TXT_NEXT, 76, 147, 1);
    display.setCursor(106, 147);
    display.print(gameNextNumber);
    display.fillRoundRect(38, 168, 124, 18, 5, GxEPD_BLACK);
    drawUiCenteredInBox(miniGameWon ? TXT_CORRECT_20 : TXT_WRONG_5, 38, 168, 124, 18, 1, GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
  }
}

void drawCoinTossGame() {
  drawMiniGameFrame(TXT_GAME_COIN);
  display.drawCircle(100, 91, 35, GxEPD_BLACK);
  display.drawCircle(100, 91, 29, GxEPD_BLACK);
  display.drawCircle(100, 91, 21, GxEPD_BLACK);
  display.drawLine(82, 76, 91, 69, GxEPD_BLACK);
  display.drawLine(109, 113, 118, 106, GxEPD_BLACK);
  display.drawLine(76, 91, 82, 91, GxEPD_BLACK);
  display.drawLine(118, 91, 124, 91, GxEPD_BLACK);
  display.setTextSize(3);
  display.setCursor(91, 81);
  if (miniGamePhase == MINI_GAME_RESULT) {
    if (useCyrillicText()) drawUiText(coinAnswer == 0 ? TXT_COIN_HEAD_MARK : TXT_COIN_TAIL_MARK, 93, 80, 2);
    else display.print(uiText(coinAnswer == 0 ? TXT_COIN_HEAD_MARK : TXT_COIN_TAIL_MARK));
  }
  else display.print("?");
  display.setTextSize(1);
  if (miniGamePhase == MINI_GAME_PICK) {
    drawChoiceButton(22, 154, 72, TXT_HEADS);
    drawChoiceButton(106, 154, 72, TXT_TAILS);
    display.fillTriangle(58, 146, 47, 135, 69, 135, GxEPD_BLACK);
    display.fillTriangle(142, 135, 131, 146, 153, 146, GxEPD_BLACK);
  } else {
    display.fillRoundRect(39, 150, 122, 18, 5, GxEPD_BLACK);
    drawUiCenteredInBox(miniGameWon ? TXT_YOU_GOT_IT : TXT_MISSED, 39, 150, 122, 18, 1, GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    drawUiCentered(miniGameWon ? TXT_PLUS_20_HAPPY : TXT_PLUS_5_HAPPY, 176, 1);
  }
}

void drawCup(int x, int y, bool selected, bool open, bool hasBall) {
  int cupY = open ? y - 14 : y;
  if (selected && !open) display.drawRoundRect(x - 23, cupY - 7, 46, 52, 8, GxEPD_BLACK);
  display.drawRoundRect(x - 18, cupY, 36, 9, 4, GxEPD_BLACK);
  display.drawLine(x - 15, cupY + 8, x - 10, cupY + 36, GxEPD_BLACK);
  display.drawLine(x + 15, cupY + 8, x + 10, cupY + 36, GxEPD_BLACK);
  display.drawLine(x - 10, cupY + 36, x + 10, cupY + 36, GxEPD_BLACK);
  display.drawLine(x - 13, cupY + 18, x + 13, cupY + 18, GxEPD_BLACK);
  display.drawLine(x - 12, cupY + 28, x + 12, cupY + 28, GxEPD_BLACK);
  display.fillRoundRect(x - 18, cupY + 39, 36, 4, 2, GxEPD_BLACK);
  if (open && hasBall) {
    display.fillCircle(x, y + 37, 5, GxEPD_BLACK);
    display.drawCircle(x, y + 37, 7, GxEPD_BLACK);
  }
}

void drawShellGame() {
  drawMiniGameFrame(TXT_GAME_SHELL);
  display.drawLine(28, 126, 172, 126, GxEPD_BLACK);
  for (byte i = 0; i < 3; i++) {
    drawCup(52 + i * 48, 77, shellPick == i, miniGamePhase == MINI_GAME_RESULT, shellAnswer == i);
  }
  display.setTextSize(1);
  if (miniGamePhase == MINI_GAME_PICK) {
    drawUiCentered(TXT_FIND_BALL, 140, 1);
    drawChoiceButton(23, 160, 58, TXT_MOVE);
    drawChoiceButton(119, 160, 58, TXT_MOVE);
  } else {
    display.fillRoundRect(38, 152, 124, 18, 5, GxEPD_BLACK);
    drawUiCenteredInBox(miniGameWon ? TXT_FOUND_20 : TXT_EMPTY_5, 38, 152, 124, 18, 1, GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
  }
}

void drawGamePlay() {
  if (activeGame == GAME_HIGH_LOW) drawHigherLowerGame();
  else if (activeGame == GAME_COIN_TOSS) drawCoinTossGame();
  else drawShellGame();
}

void drawOptions() {
  drawBookHeading(TXT_OPTIONS, TXT_CHANGE);
  drawOptionRow(0, 94, TXT_SET_CLOCK);
  drawOptionRow(1, 138, TXT_NEW_EGG);
  drawUiCentered(TXT_OPTIONS_HINT, 181, 1);
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
    else if (screen == GAME_PLAY) drawGamePlay();
    else if (screen == OPTIONS) drawOptions();
    else if (screen == GROWN_UP) drawGrownUpScreen();
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
        drawEggScaled(100, 100, frame, 100);
      } else {
        drawAnimalScaled(100, 96, animal, 1, 100);
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
  workShortcutRightCount = 0;
  workShortcutLeftCount = 0;
  saveGame(EGG);
  happyTune();
  displayDirty = true;
}

void resetWorkShortcut() {
  workShortcutRightCount = 0;
  workShortcutLeftCount = 0;
}

void enterGrownUpScreen() {
  if (pet.ageDays < PET_ADULT_DAYS) pet.ageDays = PET_ADULT_DAYS;
  pet.sleeping = false;
  screen = GROWN_UP;
  resetWorkShortcut();
  saveGame(GROWN_UP);
  displayDirty = true;
}

void checkAgeLimit() {
  if (screen != EGG && screen != GROWN_UP && pet.ageDays >= PET_ADULT_DAYS) {
    enterGrownUpScreen();
  }
}

void advanceClock() {
  gameClock.minute++;
  if (gameClock.minute < 60) return;
  gameClock.minute = 0;
  gameClock.hour++;
  if (gameClock.hour < 24) return;
  gameClock.hour = 0;
  gameClock.day++;
  if (pet.ageDays < PET_ADULT_DAYS) pet.ageDays++;
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
  if (screen != HOME && screen != ACTION_SCENE && screen != GAME_MENU && screen != GAME_PLAY) return;
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

void rewardMiniGame(bool won) {
  miniGameWon = won;
  miniGamePhase = MINI_GAME_RESULT;
  pet.happy = clampStat(pet.happy + (won ? 20 : 5));
  pet.energy = clampStat(pet.energy - 8);
  if (won) happyTune(); else chirp(300, 160);
  saveGame(HOME);
  displayDirty = true;
}

void startMiniGame(byte choice) {
  activeGame = (MiniGame)choice;
  miniGamePhase = MINI_GAME_PICK;
  miniGameWon = false;
  shellPick = 1;
  shellAnswer = random(0, 3);
  coinAnswer = random(0, 2);
  gameCurrentNumber = random(1, 10);
  do {
    gameNextNumber = random(1, 10);
  } while (gameNextNumber == gameCurrentNumber);
  screen = GAME_PLAY;
  displayDirty = true;
}

void resolveHigherLower(bool guessedHigher) {
  if (miniGamePhase != MINI_GAME_PICK) return;
  rewardMiniGame(guessedHigher ? gameNextNumber > gameCurrentNumber : gameNextNumber < gameCurrentNumber);
}

void resolveCoinToss(byte guess) {
  if (miniGamePhase != MINI_GAME_PICK) return;
  rewardMiniGame(guess == coinAnswer);
}

void resolveShellGame() {
  if (miniGamePhase != MINI_GAME_PICK) return;
  rewardMiniGame(shellPick == shellAnswer);
}

void exitMiniGame() {
  screen = HOME;
  saveGame(HOME);
  displayDirty = true;
}

void performAction(Action action) {
  if (action == PLAY) {
    resetWorkShortcut();
    gameChoice = 0;
    screen = GAME_MENU;
    displayDirty = true;
    return;
  }
  if (action == SETTINGS) {
    resetWorkShortcut();
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

bool handleWorkShortcut(bool left, bool select, bool right) {
  if (select || (left && right)) {
    resetWorkShortcut();
    return false;
  }
  if (right) {
    if (workShortcutLeftCount > 0) {
      workShortcutRightCount = 1;
      workShortcutLeftCount = 0;
    } else if (workShortcutRightCount < WORK_SHORTCUT_PRESSES) {
      workShortcutRightCount++;
    }
    return false;
  }
  if (left) {
    if (workShortcutRightCount >= WORK_SHORTCUT_PRESSES) {
      workShortcutLeftCount++;
      if (workShortcutLeftCount >= WORK_SHORTCUT_PRESSES) {
        enterGrownUpScreen();
        return true;
      }
    } else {
      resetWorkShortcut();
    }
  }
  return false;
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
  if (screen != HOME && (left || select || right)) resetWorkShortcut();
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
  if (screen == GROWN_UP) {
    if (select) {
      setupCreatesEgg = true;
      animalChoice = animal;
      editField = 0;
      screen = SELECT_ANIMAL;
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
    if (handleWorkShortcut(left, select, right)) return;
    if (left) selectedAction = (Action)((selectedAction + SETTINGS - 1) % SETTINGS);
    if (right) selectedAction = (Action)((selectedAction + 1) % SETTINGS);
    if (select) performAction(selectedAction);
    if (left || right) { chirp(900, 25); displayDirty = true; }
  } else if (screen == ACTION_SCENE && select) {
    screen = HOME;
    displayDirty = true;
  } else if (screen == GAME_MENU) {
    if (left) { gameChoice = (gameChoice + MINI_GAME_COUNT - 1) % MINI_GAME_COUNT; displayDirty = true; }
    if (right) { gameChoice = (gameChoice + 1) % MINI_GAME_COUNT; displayDirty = true; }
    if (select) {
      startMiniGame(gameChoice);
    }
  } else if (screen == GAME_PLAY) {
    if (activeGame == GAME_HIGH_LOW && miniGamePhase == MINI_GAME_PICK) {
      if (left) resolveHigherLower(false);
      if (right) resolveHigherLower(true);
    } else if (activeGame == GAME_COIN_TOSS && miniGamePhase == MINI_GAME_PICK) {
      if (left) resolveCoinToss(0);
      if (right) resolveCoinToss(1);
    } else if (activeGame == GAME_SHELL && miniGamePhase == MINI_GAME_PICK) {
      if (left) { shellPick = (shellPick + 2) % 3; displayDirty = true; }
      if (right) { shellPick = (shellPick + 1) % 3; displayDirty = true; }
    }
    if (select) {
      if (activeGame == GAME_SHELL && miniGamePhase == MINI_GAME_PICK) resolveShellGame();
      else exitMiniGame();
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
  u8g2Text.begin(display);
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
    checkAgeLimit();
    if (gameClock.minute == 0) saveGame(currentSaveStage());
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
