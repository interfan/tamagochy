/*
 * Wokwi Custom Chip API
 * Reference: https://link.wokwi.com/custom-chips-alpha
 *
 * Minimal header for building custom chips targeting wasm32-unknown-wasi.
 */
#ifndef WOKWI_API_H
#define WOKWI_API_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __wasm32__
#define WOKWI_EXPORT __attribute__((export_name))
#else
#define WOKWI_EXPORT
#endif

// Pin types
typedef uint32_t pin_t;

// Pin directions
#define INPUT  0
#define OUTPUT 1
#define ANALOG 2

// Pin value constants
#define LOW  0
#define HIGH 1

// Edge types for pin watch
typedef enum {
  RISING  = 1,
  FALLING = 2,
  BOTH    = 3,
} pin_watch_edge_t;

// Timer handle
typedef uint32_t timer_t;

// Configuration for pin watch
typedef struct {
  pin_watch_edge_t edge;
  void (*pin_change)(void *user_data, pin_t pin, uint32_t value);
  void *user_data;
} pin_watch_config_t;

// Configuration for timer
typedef struct {
  void (*callback)(void *user_data);
  void *user_data;
} timer_config_t;

// Initialize a pin
pin_t pin_init(const char *name, uint32_t direction);

// Read/write pin value
uint32_t pin_read(pin_t pin);
void pin_write(pin_t pin, uint32_t value);

// Watch for pin changes
void pin_watch(pin_t pin, const pin_watch_config_t *config);

// Timer functions
timer_t timer_init(const timer_config_t *config);
void timer_start(timer_t timer, uint64_t micros, uint64_t repeat_micros);
void timer_stop(timer_t timer);

// Simulation time in microseconds
uint64_t sim_time(void);

// Attribute reading
uint32_t attr_read(const char *name);

#endif /* WOKWI_API_H */
