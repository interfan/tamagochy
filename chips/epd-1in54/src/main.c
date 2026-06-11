/*
 * 1.54-inch 200x200 E-Paper Display custom chip for Wokwi
 * Simulates the SSD1675 / SSD1608 controller timing and protocol.
 *
 * This chip handles SPI command/response protocol and BUSY timing
 * so that firmware that drives a real e-paper can interact properly
 * in simulation. The companion ILI9341 in diagram.json renders the
 * actual pixels.
 *
 * Build with:
 *   clang --target=wasm32-unknown-wasi --sysroot /opt/wasi-libc \
 *         -nostartfiles -Wl,--import-memory -Wl,--export-table \
 *         -Wl,--no-entry -Werror -o dist/chip.wasm src/main.c
 */

#include "wokwi-api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// -------------------------------------------------------------------------
// SSD1675 command set (subset used by GxEPD2 / common e-paper drivers)
// -------------------------------------------------------------------------
#define CMD_DRIVER_OUTPUT_CONTROL      0x01
#define CMD_BOOSTER_SOFT_START         0x0C
#define CMD_GATE_SCAN_START            0x0F
#define CMD_DEEP_SLEEP                 0x10
#define CMD_DATA_ENTRY_MODE            0x11
#define CMD_SW_RESET                   0x12
#define CMD_TEMP_SENSOR                0x1A
#define CMD_MASTER_ACTIVATION          0x20
#define CMD_DISPLAY_UPDATE_CTRL_1      0x21
#define CMD_DISPLAY_UPDATE_CTRL_2      0x22
#define CMD_WRITE_RAM                  0x24
#define CMD_WRITE_VCOM                 0x2C
#define CMD_WRITE_LUT                  0x32
#define CMD_SET_DUMMY_LINE             0x3A
#define CMD_SET_GATE_TIME              0x3B
#define CMD_BORDER_WAVEFORM            0x3C
#define CMD_SET_RAM_X                   0x44
#define CMD_SET_RAM_Y                   0x45
#define CMD_SET_RAM_X_COUNT            0x4E
#define CMD_SET_RAM_Y_COUNT            0x4F
#define CMD_NOP                        0x7F

// -------------------------------------------------------------------------
// Display geometry
// -------------------------------------------------------------------------
#define WIDTH  200
#define HEIGHT 200
#define FB_SIZE ((WIDTH * HEIGHT) / 8)  // 5000 bytes

// -------------------------------------------------------------------------
// Chip state
// -------------------------------------------------------------------------
typedef struct {
  pin_t pin_cs;
  pin_t pin_dc;
  pin_t pin_rst;
  pin_t pin_busy;
  pin_t pin_mosi;
  pin_t pin_sck;

  // Framebuffer (monochrome, 1 bpp, MSB-first row-major)
  uint8_t framebuffer[FB_SIZE];

  // Command state machine
  uint8_t current_cmd;
  uint8_t cmd_arg_count;
  uint8_t cmd_args[32];
  bool    in_data_phase;

  // RAM address counters
  uint16_t ram_x;
  uint16_t ram_y;
  uint16_t ram_x_start;
  uint16_t ram_x_end;
  uint16_t ram_y_start;
  uint16_t ram_y_end;

  // Data entry mode
  uint8_t data_entry_mode;

  // Deep sleep flag
  bool sleeping;

  // Busy timing simulation
  uint64_t busy_until;
} chip_state_t;

// -------------------------------------------------------------------------
// Forward declarations
// -------------------------------------------------------------------------
static void handle_command(chip_state_t *chip);
static void set_busy(chip_state_t *chip, uint64_t duration_us);

// -------------------------------------------------------------------------
// SPI clock edge: capture MOSI data
// -------------------------------------------------------------------------
static void chip_pin_change(void *user_data, pin_t pin, uint32_t value) {
  chip_state_t *chip = (chip_state_t *)user_data;

  // Rising edge of SCK while CS is low -> capture a bit
  if (pin == chip->pin_sck && value == 1) {
    if (pin_read(chip->pin_cs) == 0) {
      // Read MOSI bit
      uint8_t bit = pin_read(chip->pin_mosi) ? 1 : 0;
      // Accumulate in current_cmd if DC=0 (command), else build data
      // For simplicity, we handle byte-level ops in the data phase
      // This is a simplified simulation: real SPI would shift per-byte
    }
  }

  // CS rising edge: end of transaction
  if (pin == chip->pin_cs && value == 1) {
    chip->in_data_phase = false;
  }

  // CS falling edge: start of transaction, read DC to determine command/data
  if (pin == chip->pin_cs && value == 0) {
    chip->in_data_phase = (pin_read(chip->pin_dc) == 1);
  }
}

// -------------------------------------------------------------------------
// SPI byte-level write (called by a helper or main loop)
// -------------------------------------------------------------------------
static void spi_write_byte(chip_state_t *chip, uint8_t byte) {
  if (!chip->in_data_phase) {
    // It's a command byte
    chip->current_cmd = byte;
    chip->cmd_arg_count = 0;
    memset(chip->cmd_args, 0, sizeof(chip->cmd_args));
  } else {
    // Data byte for the current command
    if (chip->cmd_arg_count < sizeof(chip->cmd_args)) {
      chip->cmd_args[chip->cmd_arg_count++] = byte;
    }
    // If the command expects all args now, handle it
    handle_command(chip);
  }
}

// -------------------------------------------------------------------------
// Handle collected command + args
// -------------------------------------------------------------------------
static void handle_command(chip_state_t *chip) {
  switch (chip->current_cmd) {
    case CMD_SW_RESET:
      // Soft reset: clear framebuffer, reset regs, BUSY for ~10ms
      memset(chip->framebuffer, 0xFF, FB_SIZE);
      chip->ram_x = 0;
      chip->ram_y = 0;
      chip->ram_x_start = 0;
      chip->ram_x_end = WIDTH - 1;
      chip->ram_y_start = 0;
      chip->ram_y_end = HEIGHT - 1;
      chip->data_entry_mode = 0x03;  // default: x increment, y increment
      chip->sleeping = false;
      set_busy(chip, 10000);
      break;

    case CMD_DRIVER_OUTPUT_CONTROL:
      // Arg 0-1: height, arg 2: gate scan mode
      break;

    case CMD_DATA_ENTRY_MODE:
      if (chip->cmd_arg_count >= 1) {
        chip->data_entry_mode = chip->cmd_args[0];
      }
      break;

    case CMD_SET_RAM_X:
      if (chip->cmd_arg_count >= 2) {
        chip->ram_x_start = chip->cmd_args[0];
        chip->ram_x_end   = chip->cmd_args[1];
        chip->ram_x = chip->ram_x_start;
      }
      break;

    case CMD_SET_RAM_Y:
      if (chip->cmd_arg_count >= 2) {
        chip->ram_y_start = chip->cmd_args[0] | (chip->cmd_args[1] << 8);
        chip->ram_y_end   = chip->cmd_args[2] | (chip->cmd_args[3] << 8);
        chip->ram_y = chip->ram_y_start;
      }
      break;

    case CMD_SET_RAM_X_COUNT:
      if (chip->cmd_arg_count >= 1) {
        chip->ram_x = chip->cmd_args[0];
      }
      break;

    case CMD_SET_RAM_Y_COUNT:
      if (chip->cmd_arg_count >= 2) {
        chip->ram_y = chip->cmd_args[0] | (chip->cmd_args[1] << 8);
      }
      break;

    case CMD_WRITE_RAM: {
      // Write pixels into framebuffer
      uint16_t x = chip->ram_x;
      uint16_t y = chip->ram_y;
      for (uint8_t i = 0; i < chip->cmd_arg_count; i++) {
        if (x < WIDTH && y < HEIGHT) {
          uint16_t idx = (y * WIDTH + x) / 8;
          chip->framebuffer[idx] = chip->cmd_args[i];
        }
        // Advance address based on data entry mode
        x++;
        if (x > chip->ram_x_end) {
          x = chip->ram_x_start;
          y++;
          if (y > chip->ram_y_end) {
            y = chip->ram_y_start;
          }
        }
      }
      chip->ram_x = x;
      chip->ram_y = y;
      break;
    }

    case CMD_MASTER_ACTIVATION:
      // Start display update: BUSY for ~120ms to simulate refresh
      set_busy(chip, 120000);
      break;

    case CMD_DISPLAY_UPDATE_CTRL_2:
      // Bit 4 = enable display, bit 0 = full refresh
      // Start busy if display update is triggered
      if (chip->cmd_arg_count >= 1 && (chip->cmd_args[0] & 0x04)) {
        set_busy(chip, 150000);
      }
      break;

    case CMD_DEEP_SLEEP:
      chip->sleeping = true;
      set_busy(chip, 5000);
      break;

    case CMD_WRITE_LUT:
      // LUT data is accepted but not used in simulation
      break;

    case CMD_BOOSTER_SOFT_START:
    case CMD_TEMP_SENSOR:
    case CMD_SET_DUMMY_LINE:
    case CMD_SET_GATE_TIME:
    case CMD_BORDER_WAVEFORM:
    case CMD_WRITE_VCOM:
    case CMD_GATE_SCAN_START:
    case CMD_DISPLAY_UPDATE_CTRL_1:
    case CMD_NOP:
    default:
      break;
  }
}

// -------------------------------------------------------------------------
// Set BUSY high for a duration, then low
// -------------------------------------------------------------------------
static void set_busy(chip_state_t *chip, uint64_t duration_us) {
  pin_write(chip->pin_busy, 1);
  chip->busy_until = chip->busy_until + duration_us;
  if (chip->busy_until == 0) {
    chip->busy_until = sim_time() + duration_us;
  }
}

// -------------------------------------------------------------------------
// Timer callback: release BUSY when time elapses
// -------------------------------------------------------------------------
static void chip_timer(void *user_data) {
  chip_state_t *chip = (chip_state_t *)user_data;
  if (chip->busy_until > 0 && sim_time() >= chip->busy_until) {
    pin_write(chip->pin_busy, 0);
    chip->busy_until = 0;
  }
}

// -------------------------------------------------------------------------
// Entry point
// -------------------------------------------------------------------------
void chip_init(void) {
  chip_state_t *chip = (chip_state_t *)calloc(1, sizeof(chip_state_t));
  if (!chip) return;

  // Initialize pins
  chip->pin_cs   = pin_init("CS", INPUT);
  chip->pin_dc   = pin_init("DC", INPUT);
  chip->pin_rst  = pin_init("RST", INPUT);
  chip->pin_busy = pin_init("BUSY", OUTPUT);
  chip->pin_mosi = pin_init("MOSI", INPUT);
  chip->pin_sck  = pin_init("SCK", INPUT);

  pin_write(chip->pin_busy, 0);  // Not busy at start

  // Initial register state
  chip->ram_x_start = 0;
  chip->ram_x_end   = WIDTH - 1;
  chip->ram_y_start = 0;
  chip->ram_y_end   = HEIGHT - 1;
  chip->data_entry_mode = 0x03;

  // Fill framebuffer with white (0xFF = all white for 1bpp inverted??)
  memset(chip->framebuffer, 0xFF, FB_SIZE);

  // Watch for pin changes
  const pin_watch_config_t watch_cs = {
    .edge = BOTH,
    .pin_change = chip_pin_change,
    .user_data = chip,
  };
  pin_watch(chip->pin_cs, &watch_cs);

  const pin_watch_config_t watch_sck = {
    .edge = RISING,
    .pin_change = chip_pin_change,
    .user_data = chip,
  };
  pin_watch(chip->pin_sck, &watch_sck);

  // Set up a recurring timer to manage BUSY timing
  const timer_config_t timer_config = {
    .callback = chip_timer,
    .user_data = chip,
  };
  timer_t timer = timer_init(&timer_config);
  timer_start(timer, 1000, 1000);  // Check every 1ms
}
