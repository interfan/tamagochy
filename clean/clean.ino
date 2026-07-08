/*
  One-purpose e-paper cleaner for the nRF52840 Pro Micro / V1940 board.

  Upload this sketch to clear the 1.54" e-paper panel to white. It does not
  run the Tamagotchi app.
*/

#include <SPI.h>
#include <Adafruit_GFX.h>
#include <GxEPD2_BW.h>

#if defined(ARDUINO_ARCH_NRF52)
#include <nrf_gpio.h>
#endif

#ifndef EPD_CS_PIN
#define EPD_CS_PIN 1     // Pro Micro D5/CS, raw nRF P0.24
#endif
#ifndef EPD_DC_PIN
#define EPD_DC_PIN 18    // Pro Micro D15/A0, raw nRF P0.02
#endif
#ifndef EPD_RST_PIN
#define EPD_RST_PIN 20   // Pro Micro D16/A1, raw nRF P0.29
#endif
#ifndef EPD_BUSY_PIN
#define EPD_BUSY_PIN 21  // Pro Micro D17/A2, raw nRF P0.31
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

GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> display(
    GxEPD2_154_D67(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

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

void setExternalVccPower(bool enabled) {
#if defined(ARDUINO_ARCH_NRF52)
  nrf_gpio_cfg_output(NICE_NANO_VCC_SWITCH_NRF_PIN);
  if (enabled) nrf_gpio_pin_set(NICE_NANO_VCC_SWITCH_NRF_PIN);
  else nrf_gpio_pin_clear(NICE_NANO_VCC_SWITCH_NRF_PIN);
#else
  (void)enabled;
#endif
}

void setup() {
  disableBoardIndicators();
  setExternalVccPower(true);
  delay(20);

  SPI.setPins(EPD_MISO_PIN, EPD_SCK_PIN, EPD_MOSI_PIN);
  display.init(115200);
  display.setRotation(0);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());
  display.hibernate();

  setExternalVccPower(false);
  disableBoardIndicators();
}

void loop() {
  disableBoardIndicators();
  delay(1000);
}
