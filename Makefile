# ultrausbt-Apple-ADB-adapter — top-level build
# Builds the Pico firmware (QuokkADB) in src/firmware.
#
# Prerequisites:
#   - Raspberry Pi Pico SDK installed, or leave unset to use a one-time clone under .pico-sdk/
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

build: $(OUTPUT_DIR)/ultrausbt-Apple-ADB-adapter-firmware.uf2

$(OUTPUT_DIR)/ultrausbt-Apple-ADB-adapter-firmware.uf2: $(BUILD_DIR)/Makefile
	$(MAKE) -C $(BUILD_DIR)
	@echo ""
	@echo "Build complete. Outputs in $(OUTPUT_DIR)/"
	@echo "  UF2 for flashing: $(OUTPUT_DIR)/ultrausbt-Apple-ADB-adapter-firmware.uf2"

$(BUILD_DIR)/Makefile:
	mkdir -p $(BUILD_DIR)
	./scripts/cmake_with_pico_sdk.sh -B $(BUILD_DIR) -S $(FIRMWARE_DIR)

clean:
	rm -rf $(BUILD_DIR)
	@echo "Cleaned $(BUILD_DIR)"

help:
	@echo "ultrausbt-Apple-ADB-adapter build targets:"
	@echo "  make / make build   Build Pico firmware (default)"
	@echo "  make clean          Remove build directory"
	@echo "  make help           Show this help"
	@echo ""
	@echo "Environment:"
	@echo "  PICO_SDK_PATH       Optional; otherwise SDK is cloned once to .pico-sdk/pico-sdk"
	@echo "  PICO_SDK_TAG        SDK tag when cloning (default 2.2.0)"
	@echo "  PICOTOOL_FETCH_FROM_GIT_PATH  Optional; defaults to .pico-sdk for shared picotool"
