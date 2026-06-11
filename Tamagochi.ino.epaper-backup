/*
  Mega 2560 E-Paper Tamagotchi

  Controls everywhere:
    D2 = LEFT / previous / decrease
    D3 = SELECT / confirm
    D4 = RIGHT / next / increase

  The software clock and hatch timer advance while the Mega is powered.
*/

#include <Adafruit_GFX.h>
#include <EEPROM.h>
#include <GxEPD2_BW.h>

const byte LEFT_PIN = 2;
const byte SELECT_PIN = 3;
const byte RIGHT_PIN = 4;
const byte BUZZER_PIN = 8;
const byte EPD_CS_PIN = 53;
const byte EPD_DC_PIN = 49;
const byte EPD_RST_PIN = 48;
const byte EPD_BUSY_PIN = 47;

const unsigned long DEBOUNCE_MS = 35;
const unsigned long CLOCK_TICK_MS = 60000UL;
const unsigned long NEEDS_TICK_MS = 20UL * 60000UL;
const unsigned long EGG_FRAME_MS = 15UL * 60000UL;
const uint32_t SAVE_MAGIC = 0x54414D41UL;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

enum Screen : byte { SET_CLOCK, SET_DATE, SELECT_ANIMAL, EGG, HATCHING, HOME, ACTION_SCENE, GAME_MENU, OPTIONS };
enum Animal : byte { CAT, DOG, BUNNY };
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

void playTune(const int *notes, const byte *lengths, byte count) {
  for (byte i = 0; i < count; i++) {
    tone(BUZZER_PIN, notes[i], lengths[i]);
    delay(lengths[i] + 35);
  }
  noTone(BUZZER_PIN);
}

void happyTune() {
  const int notes[] = {523, 659, 784};
  const byte lengths[] = {90, 90, 150};
  playTune(notes, lengths, 3);
}

void hatchTune() {
  const int notes[] = {523, 659, 784, 1047, 784, 1047};
  const byte lengths[] = {100, 100, 100, 220, 100, 300};
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
}

bool loadGame() {
  SaveData data;
  EEPROM.get(0, data);
  if (data.magic != SAVE_MAGIC || data.animal > BUNNY || data.stage > HOME) {
    return false;
  }
  gameClock = data.clock;
  pet = data.pet;
  animal = (Animal)data.animal;
  hatchMinutesLeft = data.hatchMinutesLeft;
  screen = data.stage == EGG ? EGG : HOME;
  return true;
}

void drawCentered(const __FlashStringHelper *text, int y, byte size = 1) {
  int16_t x1, y1;
  uint16_t w, h;
  display.setTextSize(size);
  display.getTextBounds(text, 0, y, &x1, &y1, &w, &h);
  display.setCursor((200 - w) / 2, y);
  display.print(text);
}

void drawClock() {
  display.setTextSize(1);
  display.setCursor(5, 5);
  if (gameClock.hour < 10) display.print('0');
  display.print(gameClock.hour);
  display.print(':');
  if (gameClock.minute < 10) display.print('0');
  display.print(gameClock.minute);
  display.setCursor(132, 5);
  if (gameClock.day < 10) display.print('0');
  display.print(gameClock.day);
  display.print('/');
  if (gameClock.month < 10) display.print('0');
  display.print(gameClock.month);
  display.drawLine(0, 16, 199, 16, GxEPD_BLACK);
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
  int lean = frame == 1 ? -5 : frame == 2 ? 5 : 0;
  display.fillEllipse(x + lean, y, 29, 37, GxEPD_BLACK);
  display.fillEllipse(x + lean, y, 25, 33, GxEPD_WHITE);
  display.fillTriangle(x - 18 + lean, y + 5, x - 6 + lean, y - 3, x + 2 + lean, y + 6, GxEPD_BLACK);
  display.fillTriangle(x + 1 + lean, y + 6, x + 10 + lean, y - 3, x + 18 + lean, y + 7, GxEPD_BLACK);
}

void drawAnimal(int x, int y, Animal kind, byte pose) {
  int bounce = pose % 2 ? -3 : 0;
  y += bounce;
  if (kind == CAT) {
    display.fillTriangle(x - 25, y - 23, x - 9, y - 36, x - 6, y - 17, GxEPD_BLACK);
    display.fillTriangle(x + 25, y - 23, x + 9, y - 36, x + 6, y - 17, GxEPD_BLACK);
  } else if (kind == DOG) {
    display.fillRoundRect(x - 31, y - 27, 14, 29, 6, GxEPD_BLACK);
    display.fillRoundRect(x + 17, y - 27, 14, 29, 6, GxEPD_BLACK);
  } else {
    display.fillRoundRect(x - 19, y - 51, 13, 35, 6, GxEPD_BLACK);
    display.fillRoundRect(x + 6, y - 51, 13, 35, 6, GxEPD_BLACK);
  }
  display.fillCircle(x, y, 29, GxEPD_BLACK);
  display.fillCircle(x, y, 25, GxEPD_WHITE);
  if (pet.sleeping) {
    display.drawLine(x - 14, y - 5, x - 5, y - 5, GxEPD_BLACK);
    display.drawLine(x + 5, y - 5, x + 14, y - 5, GxEPD_BLACK);
  } else {
    display.fillCircle(x - 10, y - 6, 3, GxEPD_BLACK);
    display.fillCircle(x + 10, y - 6, 3, GxEPD_BLACK);
  }
  if (kind == DOG) display.fillCircle(x, y + 4, 4, GxEPD_BLACK);
  else {
    display.drawLine(x - 3, y + 2, x, y + 5, GxEPD_BLACK);
    display.drawLine(x + 3, y + 2, x, y + 5, GxEPD_BLACK);
  }
  display.drawLine(x, y + 6, x - 7, y + 11, GxEPD_BLACK);
  display.drawLine(x, y + 6, x + 7, y + 11, GxEPD_BLACK);
  display.fillRoundRect(x - 21, y + 28, 42, 32, 14, GxEPD_BLACK);
  display.fillRect(x - 17, y + 30, 34, 22, GxEPD_WHITE);
  display.fillRoundRect(x - 24, y + 48, 19, 10, 4, GxEPD_BLACK);
  display.fillRoundRect(x + 5, y + 48, 19, 10, 4, GxEPD_BLACK);
}

void drawActionIcon(Action action, int x, int y, bool selected) {
  if (selected) display.drawRoundRect(x - 3, y - 3, 42, 37, 5, GxEPD_BLACK);
  int cx = x + 18;
  switch (action) {
    case FEED: display.fillTriangle(cx, y + 3, x + 5, y + 26, x + 31, y + 26, GxEPD_BLACK); break;
    case WATER: display.fillCircle(cx, y + 20, 8, GxEPD_BLACK); display.fillTriangle(cx, y + 1, x + 10, y + 20, x + 26, y + 20, GxEPD_BLACK); break;
    case PLAY: display.drawCircle(cx, y + 15, 11, GxEPD_BLACK); display.drawLine(x + 8, y + 15, x + 28, y + 15, GxEPD_BLACK); break;
    case SLEEP: display.fillCircle(cx, y + 14, 12, GxEPD_BLACK); display.fillCircle(cx + 6, y + 9, 12, GxEPD_WHITE); break;
    case OVERNIGHT: display.fillCircle(x + 10, y + 11, 7, GxEPD_BLACK); display.drawLine(x + 22, y + 6, x + 31, y + 6, GxEPD_BLACK); display.drawLine(x + 22, y + 13, x + 29, y + 13, GxEPD_BLACK); break;
    case CLEAN: display.fillRect(x + 8, y + 10, 20, 16, GxEPD_BLACK); display.drawLine(x + 6, y + 7, x + 30, y + 7, GxEPD_BLACK); break;
    case MEDICINE: display.drawRoundRect(x + 7, y + 5, 22, 20, 6, GxEPD_BLACK); display.drawLine(cx, y + 6, cx, y + 24, GxEPD_BLACK); break;
    case LEARN: display.drawRect(x + 5, y + 5, 13, 21, GxEPD_BLACK); display.drawRect(x + 18, y + 5, 13, 21, GxEPD_BLACK); break;
    case PET_ACTION: drawHeart(x + 11, y + 8); break;
    case GROOM: display.drawLine(x + 8, y + 6, x + 27, y + 25, GxEPD_BLACK); display.drawLine(x + 12, y + 4, x + 31, y + 23, GxEPD_BLACK); break;
    case WASH: display.drawCircle(cx, y + 16, 11, GxEPD_BLACK); display.drawCircle(x + 7, y + 7, 3, GxEPD_BLACK); break;
    case SETTINGS: display.drawCircle(cx, y + 15, 10, GxEPD_BLACK); display.fillCircle(cx, y + 15, 3, GxEPD_BLACK); break;
    default: break;
  }
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
    case LEARN: display.print(F("LEARN")); break;
    case PET_ACTION: display.print(F("PET")); break;
    case GROOM: display.print(F("GROOM")); break;
    case WASH: display.print(F("WASH")); break;
    case SETTINGS: display.print(F("OPTIONS")); break;
    default: break;
  }
}

void drawHome() {
  drawClock();
  display.setTextSize(1);
  display.setCursor(5, 21);
  display.print(F("F")); display.print(pet.food);
  display.setCursor(43, 21);
  display.print(F("W")); display.print(pet.water);
  display.setCursor(83, 21);
  display.print(F("H")); display.print(pet.happy);
  display.setCursor(125, 21);
  display.print(F("E")); display.print(pet.energy);
  display.setCursor(164, 21);
  display.print(F("+")); display.print(pet.health);
  drawAnimal(100, 83, animal, gameClock.minute);
  if (pet.poop) {
    display.fillCircle(156, 121, 7, GxEPD_BLACK);
    display.fillTriangle(149, 121, 156, 108, 163, 121, GxEPD_BLACK);
  }
  if (pet.sick) {
    drawVirus(38, 71);
  }
  if (pet.dirty) {
    display.drawLine(149, 68, 158, 60, GxEPD_BLACK);
    display.drawLine(158, 68, 167, 60, GxEPD_BLACK);
    display.drawLine(167, 68, 176, 60, GxEPD_BLACK);
  }
  display.drawLine(0, 143, 199, 143, GxEPD_BLACK);
  Action previous = (Action)((selectedAction + ACTION_COUNT - 1) % ACTION_COUNT);
  Action next = (Action)((selectedAction + 1) % ACTION_COUNT);
  drawActionIcon(previous, 10, 151, false);
  drawActionIcon(selectedAction, 81, 149, true);
  drawActionIcon(next, 152, 151, false);
  display.setTextSize(1);
  display.setCursor(80, 190);
  printActionName(selectedAction);
}

void drawSetupNumber(const __FlashStringHelper *title, int value, const __FlashStringHelper *hint) {
  drawCentered(title, 24, 2);
  display.drawRoundRect(51, 68, 98, 58, 8, GxEPD_BLACK);
  display.setTextSize(value > 99 ? 3 : 4);
  display.setCursor(value > 99 ? 64 : value < 10 ? 86 : 72, value > 99 ? 87 : 82);
  if (value < 10) display.print('0');
  display.print(value);
  drawCentered(hint, 153);
  drawCentered(F("< CHANGE   SELECT   CHANGE >"), 180);
}

void drawSetupScreen() {
  if (screen == SET_CLOCK) {
    drawSetupNumber(editField == 0 ? F("SET HOUR") : F("SET MINUTE"),
                    editField == 0 ? gameClock.hour : gameClock.minute,
                    editField == 0 ? F("Select confirms hour") : F("Select continues to date"));
  } else if (screen == SET_DATE) {
    if (editField == 0) drawSetupNumber(F("SET DAY"), gameClock.day, F("Select confirms day"));
    else if (editField == 1) drawSetupNumber(F("SET MONTH"), gameClock.month, F("Select confirms month"));
    else drawSetupNumber(F("SET YEAR"), gameClock.year, F("Select chooses animal"));
  } else {
    drawCentered(F("SELECT ANIMAL"), 20, 2);
    drawAnimal(100, 94, (Animal)animalChoice, 0);
    drawCentered(animalChoice == CAT ? F("CAT") : animalChoice == DOG ? F("DOG") : F("BUNNY"), 165, 2);
    drawCentered(F("< CHANGE   SELECT   CHANGE >"), 190);
  }
}

void drawEggScreen() {
  drawClock();
  drawCentered(F("A NEW FRIEND IS COMING"), 28);
  drawEgg(100, 92, eggFrame);
  display.setTextSize(1);
  display.setCursor(56, 145);
  display.print(F("Hatches in about"));
  display.setTextSize(2);
  display.setCursor(63, 160);
  display.print((hatchMinutesLeft + 59) / 60);
  display.print(F(" hour"));
  if (hatchMinutesLeft > 60) display.print('s');
  drawCentered(F("Keep the Mega powered"), 187);
}

void drawScene() {
  drawClock();
  drawAnimal(100, 83, animal, sceneFrame);
  display.setTextSize(2);
  display.setCursor(70, 158);
  printActionName(sceneAction);
  drawCentered(F("SELECT to return"), 188);
  switch (sceneAction) {
    case FEED: display.fillTriangle(38 + sceneFrame * 5, 83, 18 + sceneFrame * 5, 115, 58 + sceneFrame * 5, 115, GxEPD_BLACK); break;
    case WATER: display.fillCircle(35 + sceneFrame * 6, 104, 12, GxEPD_BLACK); display.fillTriangle(35 + sceneFrame * 6, 72, 22 + sceneFrame * 6, 105, 48 + sceneFrame * 6, 105, GxEPD_BLACK); break;
    case SLEEP: display.setTextSize(2); display.setCursor(142 + sceneFrame * 5, 57 - sceneFrame * 6); display.print(F("Zz")); break;
    case OVERNIGHT: display.fillCircle(35, 75, 17, GxEPD_BLACK); display.fillCircle(42 + sceneFrame * 3, 69, 17, GxEPD_WHITE); break;
    case CLEAN: display.drawLine(145 - sceneFrame * 8, 70, 170 - sceneFrame * 8, 116, GxEPD_BLACK); display.fillTriangle(158 - sceneFrame * 8, 104, 177 - sceneFrame * 8, 114, 149 - sceneFrame * 8, 121, GxEPD_BLACK); break;
    case MEDICINE: display.drawRoundRect(145 - sceneFrame * 7, 82, 35, 17, 8, GxEPD_BLACK); display.drawLine(162 - sceneFrame * 7, 83, 162 - sceneFrame * 7, 98, GxEPD_BLACK); drawVirus(35, 76); break;
    case LEARN: display.drawRect(20, 68 - sceneFrame * 3, 44, 48, GxEPD_BLACK); display.drawLine(42, 68 - sceneFrame * 3, 42, 116 - sceneFrame * 3, GxEPD_BLACK); break;
    case PET_ACTION: drawHeart(28, 76 - sceneFrame * 8); drawHeart(155, 62 + sceneFrame * 5); break;
    case GROOM: display.drawLine(25 + sceneFrame * 8, 68, 60 + sceneFrame * 8, 116, GxEPD_BLACK); display.drawLine(32 + sceneFrame * 8, 63, 67 + sceneFrame * 8, 111, GxEPD_BLACK); break;
    case WASH: for (byte i = 0; i < 6; i++) display.drawCircle(25 + i * 27, 60 + ((i + sceneFrame) % 2) * 16, 7, GxEPD_BLACK); break;
    default: break;
  }
}

void drawGameMenu() {
  drawCentered(F("CHOOSE A GAME"), 25, 2);
  display.drawRoundRect(17, 65, 166, 38, 6, GxEPD_BLACK);
  display.drawRoundRect(17, 115, 166, 38, 6, GxEPD_BLACK);
  display.setTextSize(2);
  display.setCursor(42, 77);
  display.print(F("GUESS SIDE"));
  display.setCursor(54, 127);
  display.print(F("STOP BAR"));
  display.drawRect(20, gameChoice == 0 ? 68 : 118, 8, 30, GxEPD_BLACK);
  drawCentered(F("< choose   SELECT play   choose >"), 181);
}

void drawOptions() {
  drawCentered(F("OPTIONS"), 22, 2);
  display.setTextSize(2);
  display.setCursor(45, 68);
  display.print(editField == 0 ? F("> SET CLOCK") : F("  SET CLOCK"));
  display.setCursor(45, 103);
  display.print(editField == 1 ? F("> SET DATE") : F("  SET DATE"));
  display.setCursor(45, 138);
  display.print(editField == 2 ? F("> NEW EGG") : F("  NEW EGG"));
  drawCentered(F("< move   SELECT   move >"), 184);
}

void refreshDisplay() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    display.setTextColor(GxEPD_BLACK);
    if (screen == SET_CLOCK || screen == SET_DATE || screen == SELECT_ANIMAL) drawSetupScreen();
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
  for (sceneFrame = 0; sceneFrame < 2; sceneFrame++) {
    refreshDisplay();
    chirp(650 + sceneFrame * 180, 70);
    delay(180);
  }
  sceneFrame = 1;
}

void animateEggHatch() {
  screen = HATCHING;
  for (byte frame = 0; frame < 4; frame++) {
    display.setFullWindow();
    display.firstPage();
    do {
      display.fillScreen(GxEPD_WHITE);
      drawCentered(frame < 3 ? F("CRACK...") : F("HELLO!"), 24, 2);
      if (frame < 3) {
        drawEgg(100, 100, frame);
        for (byte i = 0; i < frame + 1; i++) {
          display.drawLine(92 + i * 8, 74 + i * 7, 101 + i * 5, 85 + i * 8, GxEPD_BLACK);
        }
      } else {
        drawAnimal(100, 95, animal, 1);
        drawHeart(36, 64);
        drawHeart(153, 72);
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
  displayDirty = true;
}

void changeSetupValue(int direction) {
  if (screen == SET_CLOCK) {
    if (editField == 0) gameClock.hour = (gameClock.hour + 24 + direction) % 24;
    else gameClock.minute = (gameClock.minute + 60 + direction) % 60;
  } else if (screen == SET_DATE) {
    if (editField == 0) gameClock.day = constrain(gameClock.day + direction, 1, daysInMonth(gameClock.month, gameClock.year));
    else if (editField == 1) {
      gameClock.month = constrain(gameClock.month + direction, 1, 12);
      gameClock.day = min(gameClock.day, daysInMonth(gameClock.month, gameClock.year));
    } else gameClock.year = constrain(gameClock.year + direction, 2024, 2099);
  } else if (screen == SELECT_ANIMAL) {
    animalChoice = (animalChoice + 3 + direction) % 3;
  }
  displayDirty = true;
}

void handleButtons(bool left, bool select, bool right) {
  if (screen == SET_CLOCK || screen == SET_DATE || screen == SELECT_ANIMAL) {
    if (left) changeSetupValue(-1);
    if (right) changeSetupValue(1);
    if (select) {
      editField++;
      if (screen == SET_CLOCK && editField > 1) {
        screen = setupCreatesEgg ? SET_DATE : HOME;
        editField = 0;
        if (!setupCreatesEgg) saveGame(HOME);
      }
      else if (screen == SET_DATE && editField > 2) {
        screen = setupCreatesEgg ? SELECT_ANIMAL : HOME;
        editField = 0;
        if (!setupCreatesEgg) saveGame(HOME);
      }
      else if (screen == SELECT_ANIMAL) { animal = (Animal)animalChoice; startEgg(); }
      displayDirty = true;
    }
    return;
  }
  if (screen == HOME) {
    if (left) selectedAction = (Action)((selectedAction + ACTION_COUNT - 1) % ACTION_COUNT);
    if (right) selectedAction = (Action)((selectedAction + 1) % ACTION_COUNT);
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
    if (left) editField = (editField + 2) % 3;
    if (right) editField = (editField + 1) % 3;
    if (select) {
      setupCreatesEgg = editField == 2;
      if (editField == 0) { screen = SET_CLOCK; editField = 0; }
      else if (editField == 1) { screen = SET_DATE; editField = 0; }
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
  randomSeed(analogRead(A0));
  display.init(115200);
  display.setRotation(0);
  if (!loadGame()) {
    screen = SET_CLOCK;
    editField = 0;
    setupCreatesEgg = true;
  }
  lastClockTick = millis();
  lastNeedsTick = millis();
  lastEggFrame = millis();
  refreshDisplay();
}

void loop() {
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
