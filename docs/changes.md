# HIDHopper ADB – Changes

## GPIO configuration

### ADB pins (updated)

| Signal    | GPIO | Direction | Description           |
|----------|------|-----------|-----------------------|
| ADB out  | **18** | Output  | ADB data line to host |
| ADB in   | **19** | Input   | ADB data line from host |

Defined in `src/firmware/lib/QuokkADB/include/quokkadb_gpio.h` as `ADB_OUT_GPIO` and `ADB_IN_GPIO`.

### Other GPIOs (unchanged)

| GPIO | Name / use        | Direction | Description                    |
|------|-------------------|-----------|--------------------------------|
| 15   | LED_GPIO          | Output    | Status LED                     |
| 16   | UART_TX_GPIO      | UART TX   | Debug UART (115200 baud)       |
| 21   | ADB_PWR_GPIO      | —         | ADB power (defined, not used in init) |
| 22   | GPIO_TEST         | —         | Test pin (defined only)        |
| 23   | PICO_SMPS_MODE_PIN | —       | SMPS mode (board default)     |

Ensure hardware is wired for ADB data out on **GPIO 18** and ADB data in on **GPIO 19**.
