/**
 * SSD1306 OLED display configuration for HIDHopper ADB
 *
 * I2C pins chosen to avoid collision with existing GPIOs:
 *   ADB: 18 (out), 19 (in); LED: 15; UART: 16; ADB PWR: 21; GPIO_TEST: 22
 *   Display I2C: 4 (SDA), 5 (SCL) on i2c0 — no collision.
 *   Buttons: 6 (left), 7 (middle), 8 (right) — no collision.
 */
#ifndef HIDHOPPER_DISPLAY_CONFIG_H
#define HIDHOPPER_DISPLAY_CONFIG_H

/* I2C instance (i2c0) and pins; no collision with ADB/LED/UART GPIOs */
#define SSD1306_I2C       i2c0
#define SSD1306_PIN_SDA   4
#define SSD1306_PIN_SCL   5
#define SSD1306_ADDR      0x3c
#define SSD1306_WIDTH     128
#define SSD1306_HEIGHT    64

/* Optional: GPIOs for display UI buttons (active low, pull-up) */
#define DISPLAY_GPIO_BUTTON_LEFT   6
#define DISPLAY_GPIO_BUTTON_MIDDLE 7
#define DISPLAY_GPIO_BUTTON_RIGHT  8

#endif /* HIDHOPPER_DISPLAY_CONFIG_H */
