# Makefile for Wokwi 1.54-inch E-Paper custom chip
# SPDX-FileCopyrightText: © 2025 Tamagotchi Project
# SPDX-License-Identifier: MIT

SOURCES_C   = src/main.c
SOURCES_WAT = src/main.wat
TARGET      = dist/chip-epd-1in54.chip.wasm
CHIP_JSON   = dist/chip-epd-1in54.chip.json

.PHONY: all clean

all: $(TARGET) $(CHIP_JSON)

clean:
	rm -rf dist

dist:
	mkdir -p dist

# Prefer WAT compilation (no toolchain required), fallback to C+WASI
$(TARGET): dist $(SOURCES_WAT)
	wat2wasm $(SOURCES_WAT) -o $(TARGET)

# C compilation requires WASI SDK (clang --target=wasm32-unknown-wasi)
cbuild: dist $(SOURCES_C) src/wokwi-api.h
	clang --target=wasm32-unknown-wasi \
	      --sysroot /opt/wasi-libc \
	      -nostartfiles \
	      -Wl,--import-memory \
	      -Wl,--export-table \
	      -Wl,--no-entry \
	      -Werror \
	      -Os \
	      -o $(TARGET) $(SOURCES_C)

$(CHIP_JSON): dist chip.json
	cp chip.json $(CHIP_JSON)
