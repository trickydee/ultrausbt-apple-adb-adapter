/**
 * SSD1306 OLED display configuration for ultrausbt-Apple-ADB-adapter
 *
 * I2C pins on Display1 header (GP2–GP9 per schematic); chosen to avoid ADB GPIOs:
 *   ADB: 18 (out), 19 (in); LED: 25; UART: 0/1; Reset button: on RUN header
 *   Display I2C: 4 (SDA), 5 (SCL) on i2c0 — no collision.
 *   Buttons (active low, pull-up); OLED hint glyphs use ASCII '^' and '~':
 *     ˄  Up     GP9  (ASCII '^')
 *     ˯  Down   GP8  (ASCII '~' — letter 'v' kept for version strings)
 *     #  Center GP7
 *     *  Star   GP6
 */
#ifndef BT_USB_ADB_ADAPTER_DISPLAY_CONFIG_H
#define BT_USB_ADB_ADAPTER_DISPLAY_CONFIG_H

/* I2C instance (i2c0) and pins; no collision with ADB/LED/UART GPIOs */
#define SSD1306_I2C       i2c0
#define SSD1306_PIN_SDA   4
#define SSD1306_PIN_SCL   5
#define SSD1306_ADDR      0x3c
#define SSD1306_WIDTH     128
#define SSD1306_HEIGHT    64

/* Optional: GPIOs for display UI buttons (active low, pull-up) */
#define DISPLAY_GPIO_BUTTON_UP     9  /* ˄ (OLED glyph for ASCII '^') */
#define DISPLAY_GPIO_BUTTON_DOWN   8  /* ˯ (OLED glyph for ASCII '~') */
#define DISPLAY_GPIO_BUTTON_HASH   7  /* # (center) */
#define DISPLAY_GPIO_BUTTON_STAR   6  /* * */

/* OLED-safe single-byte stand-ins for ˄ / ˯ (font remapped; 'v' stays a letter). */
#define DISPLAY_OLED_GLYPH_UP   "^"
#define DISPLAY_OLED_GLYPH_DOWN "~"

#endif /* BT_USB_ADB_ADAPTER_DISPLAY_CONFIG_H */
