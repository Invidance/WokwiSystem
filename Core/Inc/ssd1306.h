#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#define SSD1306_WIDTH  128U
#define SSD1306_HEIGHT 64U
#define SSD1306_PAGES  (SSD1306_HEIGHT / 8U)

typedef enum {
  SSD1306_PROBE = 0,
  SSD1306_INIT_COMMAND,
  SSD1306_WINDOW_COMMAND,
  SSD1306_PAGE_DATA
} ssd1306_phase_t;

typedef struct {
  ssd1306_phase_t phase;
  uint8_t command;
  uint8_t page;
  HAL_StatusTypeDef status;
  uint32_t i2c_error;
  uint32_t hal_tick;
  uint32_t count;
} ssd1306_failure_t;

typedef struct {
  I2C_HandleTypeDef *i2c;
  uint16_t address;
  uint8_t buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8U];
  uint8_t dirty_first[SSD1306_PAGES], dirty_last[SSD1306_PAGES];
  uint32_t data_bytes_sent;
  bool initialized;
  HAL_StatusTypeDef last_status;
  uint32_t last_i2c_error;
  ssd1306_failure_t failure; /* Last failure survives successful calls/re-init. */
} ssd1306_t;

/* Zero-initialize the object ONCE. Caller owns startup delay and I2C mutex. */
bool ssd1306_init(ssd1306_t *display, I2C_HandleTypeDef *i2c, uint8_t address_7bit);
void ssd1306_clear(ssd1306_t *display);
/* Replace an entire 8-pixel text row, clearing the tail of the old text. */
void ssd1306_write_line(ssd1306_t *display, uint8_t page, const char *text);
bool ssd1306_probe(ssd1306_t *display);
/* Caller locks one page at a time, releasing the bus between pages. */
bool ssd1306_update_page(ssd1306_t *display, uint8_t page);

#endif
