/**
  ******************************************************************************
  * @file    ssd1306.h
  * @brief   SSD1306 OLED 128x64 I2C Driver — Lightweight
  ******************************************************************************
  */

#ifndef SSD1306_H
#define SSD1306_H

#include "main.h"
#include <stdint.h>
#include <string.h>

/* OLED I2C Address — most modules use 0x3C */
#define SSD1306_I2C_ADDR     (0x3C << 1)

/* Screen dimensions */
#define SSD1306_WIDTH        128
#define SSD1306_HEIGHT       64

/* Initialize the OLED display */
void ssd1306_init(I2C_HandleTypeDef *hi2c);

/* Clear the screen buffer */
void ssd1306_clear(void);

/* Push buffer to display */
void ssd1306_update(void);

/* Set cursor position for text */
void ssd1306_set_cursor(uint8_t x, uint8_t y);

/* Write a single character */
void ssd1306_write_char(char ch);

/* Write a string */
void ssd1306_write_string(const char *str);

/* Turn display on/off (for power save) */
void ssd1306_display_on(void);
void ssd1306_display_off(void);

#endif /* SSD1306_H */
