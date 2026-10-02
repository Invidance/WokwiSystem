#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define SSD1306_WIDTH  128U
#define SSD1306_HEIGHT 64U

typedef struct {
  I2C_HandleTypeDef *i2c;
  uint16_t address;
  uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8U];
  bool initialized;
  HAL_StatusTypeDef last_status;
  uint32_t last_i2c_error;
} ssd1306_t;

bool ssd1306_init(ssd1306_t *display,
                  I2C_HandleTypeDef *i2c,
                  uint8_t address_7bit);
void ssd1306_clear(ssd1306_t *display);
void ssd1306_write_text(ssd1306_t *display,
                        uint8_t x,
                        uint8_t y,
                        const char *text);
bool ssd1306_update(ssd1306_t *display);

#endif
