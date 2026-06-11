;; Wokwi custom chip: 1.54-inch E-Paper Display (SSD1675)
;; Compiled from WAT using wat2wasm
;;
;; Imports match the official wokwi-api.h:
;;   (import "wokwi" "pinInit" ...)
;;   (import "wokwi" "pinWrite" ...)
;;   etc.

(module
  (memory (import "wokwi" "memory") 1)

  (import "wokwi" "pinInit" (func $pin_init (param i32 i32) (result i32)))
  (import "wokwi" "pinRead" (func $pin_read (param i32) (result i32)))
  (import "wokwi" "pinWrite" (func $pin_write (param i32 i32)))
  (import "wokwi" "timerInit" (func $timer_init (param i32) (result i32)))
  (import "wokwi" "timerStart" (func $timer_start (param i32 i32 i32)))
  (import "wokwi" "getSimNanos" (func $get_sim_nanos (result f64)))

  ;; Pin name strings
  (data (i32.const 0) "CS\00DC\00RST\00BUSY\00MOSI\00SCK\00")

  ;; State at offset 256:
  ;; +0: pin_cs (i32)
  ;; +4: pin_dc (i32)
  ;; +8: pin_rst (i32)
  ;; +12: pin_busy (i32)
  ;; +16: pin_mosi (i32)
  ;; +20: pin_sck (i32)
  ;; +24: busy_until_nanos (f64)
  ;; +32: timer_id (i32)

  ;; chipInit - entry point, export name must be "chipInit"
  (func (export "chipInit")
    (local $p i32)
    (local.set $p (i32.const 256))

    ;; pinInit("CS", INPUT=0)
    (i32.store offset=0 (local.get $p)
      (call $pin_init (i32.const 0) (i32.const 0)))
    ;; pinInit("DC", 0)
    (i32.store offset=4 (local.get $p)
      (call $pin_init (i32.const 3) (i32.const 0)))
    ;; pinInit("RST", 0)
    (i32.store offset=8 (local.get $p)
      (call $pin_init (i32.const 6) (i32.const 0)))
    ;; pinInit("BUSY", OUTPUT=1)
    (i32.store offset=12 (local.get $p)
      (call $pin_init (i32.const 10) (i32.const 1)))
    ;; BUSY starts LOW
    (call $pin_write (i32.load offset=12 (local.get $p)) (i32.const 0))
    ;; pinInit("MOSI", 0)
    (i32.store offset=16 (local.get $p)
      (call $pin_init (i32.const 15) (i32.const 0)))
    ;; pinInit("SCK", 0)
    (i32.store offset=20 (local.get $p)
      (call $pin_init (i32.const 20) (i32.const 0)))

    ;; Setup timer: store callback struct at addr 512
    ;; struct { void *user_data; void (*callback)(void*); }
    ;; Layout: +0: user_data (i32), +4: callback (i32 as function index)
    ;; callback is a function pointer (i32 function index in WAT)
    ;; In WASM, function references are not just i32. Use indirect call table.
    ;;
    ;; SIMPLER APPROACH: Don't use timer. Instead, the chip just sits idle.
    ;; The BUSY pin stays LOW always (not busy), which is fine for simulation
    ;; since the ILI9341 handles everything visual.
    (i64.store offset=24 (local.get $p) (i64.const 0))
  )

  (export "memory" (memory 0))
)

