/*
  nRF52840 + 1.54" e-paper partial refresh test

  Purpose:
    Draw one cat HOME screen and test different ways to move only the action
    selection border with partial refresh.

  Controls:
    LEFT / RIGHT  - move selected action
    SELECT        - run cat eating animation with partial refresh
    LEFT + RIGHT  - force a full cleanup refresh

  Start with PARTIAL_MODE = PARTIAL_ACTION_STRIP.
*/

#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <GxEPD2_BW.h>
#if defined(ARDUINO_ARCH_NRF52)
#include <nrf.h>
#include <nrf_gpio.h>
#endif

#include "companion_bitmaps.h"
#include "action_icons.h"

#ifndef LEFT_PIN
#define LEFT_PIN 11      // Pro Micro D1/TX, raw nRF P0.06
#endif
#ifndef SELECT_PIN
#define SELECT_PIN 12    // Pro Micro D0/RX, raw nRF P0.08
#endif
#ifndef RIGHT_PIN
#define RIGHT_PIN 33     // Pro Micro D10/NFC1, raw nRF P0.09
#endif

#ifndef EPD_CS_PIN
#define EPD_CS_PIN 1     // Pro Micro D5/CS, raw nRF P0.24
#endif
#ifndef EPD_DC_PIN
#define EPD_DC_PIN 18    // Pro Micro D19/A0, raw nRF P0.02
#endif
#ifndef EPD_RST_PIN
#define EPD_RST_PIN 20   // Pro Micro D20/A1, raw nRF P0.29
#endif
#ifndef EPD_BUSY_PIN
#define EPD_BUSY_PIN 21  // Pro Micro D21/A2, raw nRF P0.31
#endif
#ifndef EPD_MISO_PIN
#define EPD_MISO_PIN 28  // Pro Micro D3/MISO, raw nRF P0.20; dummy, not connected
#endif
#ifndef EPD_SCK_PIN
#define EPD_SCK_PIN 29   // Pro Micro D2/SCK, raw nRF P0.17
#endif
#ifndef EPD_MOSI_PIN
#define EPD_MOSI_PIN 30  // Pro Micro D4/MOSI, raw nRF P0.22
#endif

#ifndef NICE_NANO_BLUE_LED_NRF_PIN
#define NICE_NANO_BLUE_LED_NRF_PIN 15
#endif
#ifndef NICE_NANO_RED_LED_NRF_PIN
#define NICE_NANO_RED_LED_NRF_PIN 16
#endif
#ifndef NRF_STATUS_BLUE_LED_NRF_PIN
#define NRF_STATUS_BLUE_LED_NRF_PIN 42
#endif
#ifndef NRF_STATUS_RED_LED_NRF_PIN
#define NRF_STATUS_RED_LED_NRF_PIN 47
#endif
#ifndef NICE_NANO_VCC_SWITCH_NRF_PIN
#define NICE_NANO_VCC_SWITCH_NRF_PIN 13
#endif

const unsigned long DEBOUNCE_MS = 35;

const byte PARTIAL_OFF = 0;
const byte PARTIAL_TIGHT_TWO_CELLS = 1;
const byte PARTIAL_ACTION_STRIP = 2;
const byte PARTIAL_FULL_SCREEN = 3;

// Change these values for each experiment.
const byte PARTIAL_MODE = PARTIAL_ACTION_STRIP;
const bool KEEP_EPD_POWER_ON = true;
const bool HIBERNATE_AFTER_FULL_REFRESH = false;
const bool HIBERNATE_AFTER_PARTIAL_REFRESH = false;
const byte PARTIALS_BEFORE_FULL_CLEANUP = 3;
const bool EATING_ANIMATION_FULL_SCREEN_PARTIAL = false;
const unsigned long EATING_ANIMATION_FRAME_MS = 350;

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

enum Action : byte {
  FEED,
  WATER,
  PLAY,
  SLEEP,
  OVERNIGHT,
  CLEAN,
  MEDICINE,
  LEARN,
  PET_ACTION,
  GROOM,
  WASH,
  ACTION_COUNT
};

struct Button {
  byte pin;
  bool stable;
  bool last;
  unsigned long changedAt;
};

struct Rect {
  int x;
  int y;
  int w;
  int h;
};

Button leftButton = {LEFT_PIN, HIGH, HIGH, 0};
Button selectButton = {SELECT_PIN, HIGH, HIGH, 0};
Button rightButton = {RIGHT_PIN, HIGH, HIGH, 0};
Action selectedAction = FEED;
byte partialsSinceFull = 0;
bool eatingAnimationActive = false;

void setExternalVccPower(bool enabled) {
#if defined(ARDUINO_ARCH_NRF52)
  nrf_gpio_cfg_output(NICE_NANO_VCC_SWITCH_NRF_PIN);
  if (enabled) nrf_gpio_pin_set(NICE_NANO_VCC_SWITCH_NRF_PIN);
  else nrf_gpio_pin_clear(NICE_NANO_VCC_SWITCH_NRF_PIN);
#else
  (void)enabled;
#endif
}

void disableBoardIndicators() {
#if defined(ARDUINO_ARCH_NRF52)
  nrf_gpio_cfg_output(NICE_NANO_BLUE_LED_NRF_PIN);
  nrf_gpio_pin_set(NICE_NANO_BLUE_LED_NRF_PIN);
  nrf_gpio_cfg_output(NICE_NANO_RED_LED_NRF_PIN);
  nrf_gpio_pin_set(NICE_NANO_RED_LED_NRF_PIN);
  nrf_gpio_cfg_output(NRF_STATUS_BLUE_LED_NRF_PIN);
  nrf_gpio_pin_set(NRF_STATUS_BLUE_LED_NRF_PIN);
  nrf_gpio_cfg_output(NRF_STATUS_RED_LED_NRF_PIN);
  nrf_gpio_pin_set(NRF_STATUS_RED_LED_NRF_PIN);
#endif
}

void platformPeripheralPowerOn() {
  setExternalVccPower(true);
  disableBoardIndicators();
  delay(10);
}

void platformPeripheralPowerOff() {
  disableBoardIndicators();
  setExternalVccPower(false);
}

void finishDisplayTransaction(bool partial) {
  bool shouldHibernate = partial ? HIBERNATE_AFTER_PARTIAL_REFRESH : HIBERNATE_AFTER_FULL_REFRESH;
  if (!KEEP_EPD_POWER_ON && shouldHibernate) {
    display.hibernate();
    platformPeripheralPowerOff();
  } else if (shouldHibernate) {
    display.hibernate();
  }
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

void drawHeart(int x, int y) {
  display.fillCircle(x + 3, y + 3, 3, GxEPD_BLACK);
  display.fillCircle(x + 9, y + 3, 3, GxEPD_BLACK);
  display.fillTriangle(x, y + 4, x + 12, y + 4, x + 6, y + 12, GxEPD_BLACK);
}

void drawStatusIcon(byte icon, int x, int y) {
  if (icon == 0) {
    display.drawCircle(x + 8, y + 6, 5, GxEPD_BLACK);
    display.drawCircle(x + 8, y + 6, 2, GxEPD_BLACK);
    display.drawLine(x, y + 1, x, y + 11, GxEPD_BLACK);
    display.drawLine(x + 2, y + 1, x + 2, y + 11, GxEPD_BLACK);
  } else if (icon == 1) {
    display.drawLine(x + 8, y, x + 4, y + 6, GxEPD_BLACK);
    display.drawLine(x + 8, y, x + 12, y + 6, GxEPD_BLACK);
    display.drawLine(x + 4, y + 6, x + 3, y + 9, GxEPD_BLACK);
    display.drawLine(x + 12, y + 6, x + 13, y + 9, GxEPD_BLACK);
    display.drawLine(x + 3, y + 9, x + 5, y + 12, GxEPD_BLACK);
    display.drawLine(x + 13, y + 9, x + 11, y + 12, GxEPD_BLACK);
    display.drawLine(x + 5, y + 12, x + 8, y + 13, GxEPD_BLACK);
    display.drawLine(x + 8, y + 13, x + 11, y + 12, GxEPD_BLACK);
    display.drawPixel(x + 7, y + 6, GxEPD_BLACK);
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
  int fillWidth = (w - 22) * value / 100;
  if (fillWidth > 0) display.fillRoundRect(x + 20, y + 1, fillWidth, 3, 1, GxEPD_BLACK);
}

void actionIconPosition(Action action, int &x, int &y) {
  byte index = (byte)action;
  x = index < 6 ? 3 + index * 32 : 18 + (index - 6) * 36;
  y = index < 6 ? 144 : 173;
}

const uint8_t *actionBitmap(Action action) {
  switch (action) {
    case FEED: return ACTION_FOOD_ICON_BITMAP;
    case WATER: return ACTION_WATER_ICON_BITMAP;
    case PLAY: return ACTION_PLAY_ICON_BITMAP;
    case SLEEP: return ACTION_MOON_ICON_BITMAP;
    case OVERNIGHT: return ACTION_OVERNIGHT_ICON_BITMAP;
    case CLEAN: return ACTION_CLEAN_ICON_BITMAP;
    case MEDICINE: return ACTION_MEDICINE_ICON_BITMAP;
    case LEARN: return ACTION_LEARN_ICON_BITMAP;
    case PET_ACTION: return ACTION_PET_ICON_BITMAP;
    case GROOM: return ACTION_GROOM_ICON_BITMAP;
    case WASH: return ACTION_WASH_ICON_BITMAP;
    default: return ACTION_FOOD_ICON_BITMAP;
  }
}

void drawActionIcon(Action action, bool selected) {
  int x;
  int y;
  actionIconPosition(action, x, y);
  if (selected) display.drawRoundRect(x - 2, y - 2, 32, 28, 6, GxEPD_BLACK);
  display.drawBitmap(x, y, actionBitmap(action), ACTION_ICON_WIDTH, ACTION_ICON_HEIGHT, GxEPD_BLACK);
}

void drawActionCell(Action action, bool selected) {
  int x;
  int y;
  actionIconPosition(action, x, y);
  display.fillRect(x - 4, y - 4, 36, 32, GxEPD_WHITE);
  drawActionIcon(action, selected);
}

void drawActionStrip() {
  display.fillRect(0, 139, 200, 61, GxEPD_WHITE);
  for (byte i = 0; i < ACTION_COUNT; i++) {
    drawActionIcon((Action)i, selectedAction == (Action)i);
  }
}

void drawHome() {
  drawMeter(5, 8, 90, 0, 80);
  drawMeter(5, 22, 90, 1, 75);
  drawMeter(105, 8, 90, 2, 100);
  drawMeter(105, 22, 90, 3, 85);
  display.drawBitmap((200 - CAT_WIDTH) / 2, 28, CAT_BITMAP, CAT_WIDTH, CAT_HEIGHT, GxEPD_BLACK);
  drawActionStrip();
}

const uint8_t *catEatingFrame(byte frame) {
  switch (frame & 3) {
    case 0: return CAT_FEED_0_RLE;
    case 1: return CAT_FEED_1_RLE;
    case 2: return CAT_FEED_2_RLE;
    default: return CAT_FEED_3_RLE;
  }
}

void drawRleBitmap(int x, int y, const uint8_t *rle, uint16_t width, uint16_t height) {
  uint16_t px = 0;
  uint16_t py = 0;
  bool black = false;
  uint32_t index = 0;
  while (py < height) {
    uint8_t run = pgm_read_byte(rle + index++);
    if (run == 0) {
      black = !black;
      continue;
    }
    while (run > 0 && py < height) {
      uint16_t chunk = run < (width - px) ? run : (width - px);
      if (black) display.fillRect(x + px, y + py, chunk, 1, GxEPD_BLACK);
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

void drawEatingFrame(byte frame) {
  if (EATING_ANIMATION_FULL_SCREEN_PARTIAL) {
    display.fillScreen(GxEPD_WHITE);
  } else {
    display.fillRect(8, 8, CAT_ACTION_SCENE_WIDTH, CAT_ACTION_SCENE_HEIGHT, GxEPD_WHITE);
  }
  drawRleBitmap((200 - CAT_ACTION_SCENE_WIDTH) / 2,
                (200 - CAT_ACTION_SCENE_HEIGHT) / 2,
                catEatingFrame(frame),
                CAT_ACTION_SCENE_WIDTH,
                CAT_ACTION_SCENE_HEIGHT);
}

void alignWindowToByteColumns(Rect &rect) {
  int right = rect.x + rect.w;
  rect.x = max(0, rect.x & ~7);
  right = min(200, (right + 7) & ~7);
  rect.w = right - rect.x;
}

Rect partialWindowFor(Action previousAction, Action currentAction) {
  if (PARTIAL_MODE == PARTIAL_FULL_SCREEN) return {0, 0, 200, 200};
  if (PARTIAL_MODE == PARTIAL_ACTION_STRIP) return {0, 139, 200, 61};

  int oldX;
  int oldY;
  int newX;
  int newY;
  actionIconPosition(previousAction, oldX, oldY);
  actionIconPosition(currentAction, newX, newY);

  Rect rect;
  rect.x = min(oldX, newX) - 8;
  rect.y = min(oldY, newY) - 8;
  int right = max(oldX, newX) + ACTION_ICON_WIDTH + 8;
  int bottom = max(oldY, newY) + ACTION_ICON_HEIGHT + 8;
  if (rect.x < 0) rect.x = 0;
  if (rect.y < 0) rect.y = 0;
  if (right > 200) right = 200;
  if (bottom > 200) bottom = 200;
  rect.w = right - rect.x;
  rect.h = bottom - rect.y;
  alignWindowToByteColumns(rect);
  return rect;
}

void fullRefresh() {
  platformPeripheralPowerOn();
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawHome();
  } while (display.nextPage());
  finishDisplayTransaction(false);
  partialsSinceFull = 0;
  Serial.println(F("full refresh"));
}

void partialRefreshSelection(Action previousAction, Action currentAction) {
  if (PARTIAL_MODE == PARTIAL_OFF || partialsSinceFull >= PARTIALS_BEFORE_FULL_CLEANUP) {
    fullRefresh();
    return;
  }

  Rect rect = partialWindowFor(previousAction, currentAction);
  platformPeripheralPowerOn();
  display.setPartialWindow(rect.x, rect.y, rect.w, rect.h);
  display.firstPage();
  do {
    if (PARTIAL_MODE == PARTIAL_FULL_SCREEN) {
      display.fillScreen(GxEPD_WHITE);
      drawHome();
    } else if (PARTIAL_MODE == PARTIAL_ACTION_STRIP) {
      drawActionStrip();
    } else {
      drawActionCell(previousAction, false);
      drawActionCell(currentAction, true);
    }
  } while (display.nextPage());
  finishDisplayTransaction(true);
  partialsSinceFull++;

  Serial.print(F("partial "));
  Serial.print(partialsSinceFull);
  Serial.print(F(" window "));
  Serial.print(rect.x);
  Serial.print(',');
  Serial.print(rect.y);
  Serial.print(' ');
  Serial.print(rect.w);
  Serial.print('x');
  Serial.println(rect.h);
}

void partialRefreshEatingFrame(byte frame) {
  Rect rect = EATING_ANIMATION_FULL_SCREEN_PARTIAL ?
              Rect{0, 0, 200, 200} :
              Rect{8, 8, CAT_ACTION_SCENE_WIDTH, CAT_ACTION_SCENE_HEIGHT};
  alignWindowToByteColumns(rect);

  platformPeripheralPowerOn();
  display.setPartialWindow(rect.x, rect.y, rect.w, rect.h);
  display.firstPage();
  do {
    drawEatingFrame(frame);
  } while (display.nextPage());
  finishDisplayTransaction(true);

  Serial.print(F("eat frame "));
  Serial.print(frame);
  Serial.print(F(" partial window "));
  Serial.print(rect.x);
  Serial.print(',');
  Serial.print(rect.y);
  Serial.print(' ');
  Serial.print(rect.w);
  Serial.print('x');
  Serial.println(rect.h);
}

void animateEatingPartial() {
  eatingAnimationActive = true;
  Serial.println(F("partial eating animation"));
  for (byte frame = 0; frame < 4; frame++) {
    partialRefreshEatingFrame(frame);
    delay(EATING_ANIMATION_FRAME_MS);
  }
  delay(500);
  eatingAnimationActive = false;
  fullRefresh();
}

void moveSelection(int direction) {
  Action previousAction = selectedAction;
  selectedAction = (Action)((selectedAction + ACTION_COUNT + direction) % ACTION_COUNT);
  partialRefreshSelection(previousAction, selectedAction);
}

void setup() {
  pinMode(LEFT_PIN, INPUT_PULLUP);
  pinMode(SELECT_PIN, INPUT_PULLUP);
  pinMode(RIGHT_PIN, INPUT_PULLUP);

  disableBoardIndicators();
  platformPeripheralPowerOn();
  SPI.setPins(EPD_MISO_PIN, EPD_SCK_PIN, EPD_MOSI_PIN);

  Serial.begin(115200);
  delay(300);
  Serial.println(F("Partial refresh test boot"));
  Serial.print(F("PARTIAL_MODE="));
  Serial.println(PARTIAL_MODE);

  display.init(115200);
  display.setRotation(0);
  fullRefresh();
}

void loop() {
  if (eatingAnimationActive) return;
  if (digitalRead(LEFT_PIN) == LOW && digitalRead(RIGHT_PIN) == LOW) {
    fullRefresh();
    delay(400);
    return;
  }
  if (pressed(leftButton)) moveSelection(-1);
  if (pressed(rightButton)) moveSelection(1);
  if (pressed(selectButton)) animateEatingPartial();
}
