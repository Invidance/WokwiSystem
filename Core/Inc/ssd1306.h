#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

#define SSD1306_WIDTH       128
#define SSD1306_HEIGHT      64
#define SSD1306_PAGES       (SSD1306_HEIGHT / 8)

#define SSD1306_COLOR_BLACK 0
#define SSD1306_COLOR_WHITE 1

typedef struct
{
    I2C_HandleTypeDef *i2c;

    uint8_t address;

    uint8_t buffer[SSD1306_WIDTH * SSD1306_PAGES];

    uint8_t cursorX;
    uint8_t cursorY;

} SSD1306_t;


/* Initialization */
HAL_StatusTypeDef SSD1306_Init(
    SSD1306_t *display,
    I2C_HandleTypeDef *i2c,
    uint8_t address
);


/* Screen operations */
void SSD1306_Clear(SSD1306_t *display);

HAL_StatusTypeDef SSD1306_UpdateScreen(
    SSD1306_t *display
);


/* Drawing */
void SSD1306_DrawPixel(
    SSD1306_t *display,
    uint8_t x,
    uint8_t y,
    uint8_t color
);


/* Text */
void SSD1306_SetCursor(
    SSD1306_t *display,
    uint8_t x,
    uint8_t y
);

void SSD1306_WriteChar(
    SSD1306_t *display,
    char ch
);

void SSD1306_WriteString(
    SSD1306_t *display,
    const char *str
);

#endif
