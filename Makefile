# HIDHopper ADB — top-level build
# Builds the Pico firmware (QuokkADB) in src/firmware.
#
# Prerequisites:
#   - Raspberry Pi Pico SDK installed
#   - PICO_SDK_PATH set to your pico-sdk directory (or use PICO_SDK_FETCH_FROM_GIT=ON)
#
# Usage:
#   make          # build firmware
#   make clean    # remove build artifacts
#   make help     # show this help

FIRMWARE_DIR := src/firmware
BUILD_DIR    := $(FIRMWARE_DIR)/build
OUTPUT_DIR   := $(BUILD_DIR)/src

.PHONY: all build clean help

all: build

build: $(OUTPUT_DIR)/HIDHopper-firmware.uf2

$(OUTPUT_DIR)/HIDHopper-firmware.uf2: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR)
	@echo ""
	@echo "Build complete. Outputs in $(OUTPUT_DIR)/"
	@echo "  UF2 for flashing: $(OUTPUT_DIR)/HIDHopper-firmware.uf2"

$(BUILD_DIR)/Makefile:
	@if [ -z "$${PICO_SDK_PATH}" ] && [ -z "$${PICO_SDK_FETCH_FROM_GIT}" ]; then \
		echo "Error: Set PICO_SDK_PATH to your pico-sdk directory, or set PICO_SDK_FETCH_FROM_GIT=ON"; \
		exit 1; \
	fi
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake ..

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned $(BUILD_DIR)"

help:
	@echo "HIDHopper ADB build targets:"
	@echo "  make / make build   Build Pico firmware (default)"
	@echo "  make clean          Remove build directory"
	@echo "  make help           Show this help"
	@echo ""
	@echo "Environment:"
	@echo "  PICO_SDK_PATH       Path to Raspberry Pi Pico SDK (required unless using fetch)"
	@echo "  PICO_SDK_FETCH_FROM_GIT=ON  Fetch SDK from git if PICO_SDK_PATH not set"
