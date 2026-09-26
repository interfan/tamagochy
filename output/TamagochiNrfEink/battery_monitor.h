#pragma once

#include <stdint.h>

// Approximate single-cell 3.7 V Li-ion curve, NOT a calibrated Nokia fuel gauge.
// Confirm the cell model before tuning this table, especially for 4.4 V cells.
// Voltages above the last point saturate at 100%; this does not set charge voltage.
struct BatteryCurvePoint {
  uint16_t millivolts;
  uint8_t percent;
};

static const BatteryCurvePoint BATTERY_CURVE[] = {
    {3300, 0}, {3600, 10}, {3700, 20}, {3800, 40},
    {3900, 60}, {4000, 75}, {4100, 90}, {4200, 100}};

inline uint8_t batteryPercentFromMillivolts(uint16_t millivolts) {
  if (millivolts <= BATTERY_CURVE[0].millivolts) return 0;
  for (unsigned int i = 1; i < sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]); ++i) {
    const BatteryCurvePoint &low = BATTERY_CURVE[i - 1];
    const BatteryCurvePoint &high = BATTERY_CURVE[i];
    if (millivolts <= high.millivolts) {
      return low.percent + (uint32_t)(millivolts - low.millivolts) *
          (high.percent - low.percent) / (high.millivolts - low.millivolts);
    }
  }
  return 100;
}

// VDDH/5, internal 600 mV reference, gain 1/2, 12-bit conversion: 6 V full scale.
inline uint16_t batteryMillivoltsFromSamples(uint32_t sum, uint8_t count) {
  return count ? (sum * 6000UL + (uint32_t)count * 2048UL) /
      ((uint32_t)count * 4096UL) : 0;
}

struct BatteryAlertState {
  bool warned40 = false;
  bool warned20 = false;

  // Return only a newly crossed warning threshold; 20% takes priority at boot.
  uint8_t observe(uint16_t millivolts) {
    if (millivolts < 2500 || millivolts > 4500) return 0;
    uint8_t percent = batteryPercentFromMillivolts(millivolts);
    // Five percentage points of hysteresis avoid repeated alerts near a boundary.
    if (percent > 45) warned40 = false;
    if (percent > 25) warned20 = false;
    if (percent <= 20) {
      warned40 = true;
      if (!warned20) {
        warned20 = true;
        return 20;
      }
    } else if (percent <= 40 && !warned40) {
      warned40 = true;
      return 40;
    }
    return 0;
  }
};
