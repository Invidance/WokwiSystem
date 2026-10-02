/*
 * ssd1306.c
 *
 *  Created on: Oct 2, 2026
 *      Author: maks
 */
#include "ssd1306.h"
#include <string.h>

static const uint8_t font5x7[][5] =
{
    /* 32 ' ' */
    {0x00,0x00,0x00,0x00,0x00},

    /* 33 ! */
    {0x00,0x00,0x5F,0x00,0x00},

    /* 34 " */
    {0x00,0x07,0x00,0x07,0x00},

    /* 35 # */
    {0x14,0x7F,0x14,0x7F,0x14},

    /* 36 $ */
    {0x24,0x2A,0x7F,0x2A,0x12},

    /* 37 % */
    {0x23,0x13,0x08,0x64,0x62},

    /* 38 & */
    {0x36,0x49,0x55,0x22,0x50},

    /* 39 ' */
    {0x00,0x05,0x03,0x00,0x00},

    /* 40 ( */
    {0x00,0x1C,0x22,0x41,0x00},

    /* 41 ) */
    {0x00,0x41,0x22,0x1C,0x00},

    /* 42 * */
    {0x14,0x08,0x3E,0x08,0x14},

    /* 43 + */
    {0x08,0x08,0x3E,0x08,0x08},

    /* 44 , */
    {0x00,0x50,0x30,0x00,0x00},

    /* 45 - */
    {0x08,0x08,0x08,0x08,0x08},

    /* 46 . */
    {0x00,0x60,0x60,0x00,0x00},

    /* 47 / */
    {0x20,0x10,0x08,0x04,0x02},

    /* 48 0 */
    {0x3E,0x51,0x49,0x45,0x3E},

    /* 49 1 */
    {0x00,0x42,0x7F,0x40,0x00},

    /* 50 2 */
    {0x42,0x61,0x51,0x49,0x46},

    /* 51 3 */
    {0x21,0x41,0x45,0x4B,0x31},

    /* 52 4 */
    {0x18,0x14,0x12,0x7F,0x10},

    /* 53 5 */
    {0x27,0x45,0x45,0x45,0x39},

    /* 54 6 */
    {0x3C,0x4A,0x49,0x49,0x30},

    /* 55 7 */
    {0x01,0x71,0x09,0x05,0x03},

    /* 56 8 */
    {0x36,0x49,0x49,0x49,0x36},

    /* 57 9 */
    {0x06,0x49,0x49,0x29,0x1E},

    /* 58 : */
    {0x00,0x36,0x36,0x00,0x00},

    /* 59 ; */
    {0x00,0x56,0x36,0x00,0x00},

    /* 60 < */
    {0x08,0x14,0x22,0x41,0x00},

    /* 61 = */
    {0x14,0x14,0x14,0x14,0x14},

    /* 62 > */
    {0x00,0x41,0x22,0x14,0x08},

    /* 63 ? */
    {0x02,0x01,0x51,0x09,0x06},

    /* 64 @ */
    {0x32,0x49,0x79,0x41,0x3E},

    /* 65 A */
    {0x7E,0x11,0x11,0x11,0x7E},

    /* 66 B */
    {0x7F,0x49,0x49,0x49,0x36},

    /* 67 C */
    {0x3E,0x41,0x41,0x41,0x22},

    /* 68 D */
    {0x7F,0x41,0x41,0x22,0x1C},

    /* 69 E */
    {0x7F,0x49,0x49,0x49,0x41},

    /* 70 F */
    {0x7F,0x09,0x09,0x09,0x01},

    /* 71 G */
    {0x3E,0x41,0x49,0x49,0x7A},

    /* 72 H */
    {0x7F,0x08,0x08,0x08,0x7F},

    /* 73 I */
    {0x00,0x41,0x7F,0x41,0x00},

    /* 74 J */
    {0x20,0x40,0x41,0x3F,0x01},

    /* 75 K */
    {0x7F,0x08,0x14,0x22,0x41},

    /* 76 L */
    {0x7F,0x40,0x40,0x40,0x40},

    /* 77 M */
    {0x7F,0x02,0x0C,0x02,0x7F},

    /* 78 N */
    {0x7F,0x04,0x08,0x10,0x7F},

    /* 79 O */
    {0x3E,0x41,0x41,0x41,0x3E},

    /* 80 P */
    {0x7F,0x09,0x09,0x09,0x06},

    /* 81 Q */
    {0x3E,0x41,0x51,0x21,0x5E},

    /* 82 R */
    {0x7F,0x09,0x19,0x29,0x46},

    /* 83 S */
    {0x46,0x49,0x49,0x49,0x31},

    /* 84 T */
    {0x01,0x01,0x7F,0x01,0x01},

    /* 85 U */
    {0x3F,0x40,0x40,0x40,0x3F},

    /* 86 V */
    {0x1F,0x20,0x40,0x20,0x1F},

    /* 87 W */
    {0x7F,0x20,0x18,0x20,0x7F},

    /* 88 X */
    {0x63,0x14,0x08,0x14,0x63},

    /* 89 Y */
    {0x03,0x04,0x78,0x04,0x03},

    /* 90 Z */
    {0x61,0x51,0x49,0x45,0x43},

    /* 91 [ */
    {0x00,0x7F,0x41,0x41,0x00},

    /* 92 \ */
    {0x02,0x04,0x08,0x10,0x20},

    /* 93 ] */
    {0x00,0x41,0x41,0x7F,0x00},

    /* 94 ^ */
    {0x04,0x02,0x01,0x02,0x04},

    /* 95 _ */
    {0x40,0x40,0x40,0x40,0x40},

    /* 96 ` */
    {0x00,0x01,0x02,0x04,0x00},

    /* 97 a */
    {0x20,0x54,0x54,0x54,0x78},

    /* 98 b */
    {0x7F,0x48,0x44,0x44,0x38},

    /* 99 c */
    {0x38,0x44,0x44,0x44,0x20},

    /* 100 d */
    {0x38,0x44,0x44,0x48,0x7F},

    /* 101 e */
    {0x38,0x54,0x54,0x54,0x18},

    /* 102 f */
    {0x08,0x7E,0x09,0x01,0x02},

    /* 103 g */
    {0x0C,0x52,0x52,0x52,0x3E},

    /* 104 h */
    {0x7F,0x08,0x04,0x04,0x78},

    /* 105 i */
    {0x00,0x44,0x7D,0x40,0x00},

    /* 106 j */
    {0x20,0x40,0x44,0x3D,0x00},

    /* 107 k */
    {0x7F,0x10,0x28,0x44,0x00},

    /* 108 l */
    {0x00,0x41,0x7F,0x40,0x00},

    /* 109 m */
    {0x7C,0x04,0x18,0x04,0x78},

    /* 110 n */
    {0x7C,0x08,0x04,0x04,0x78},

    /* 111 o */
    {0x38,0x44,0x44,0x44,0x38},

    /* 112 p */
    {0x7C,0x14,0x14,0x14,0x08},

    /* 113 q */
    {0x08,0x14,0x14,0x18,0x7C},

    /* 114 r */
    {0x7C,0x08,0x04,0x04,0x08},

    /* 115 s */
    {0x48,0x54,0x54,0x54,0x20},

    /* 116 t */
    {0x04,0x3F,0x44,0x40,0x20},

    /* 117 u */
    {0x3C,0x40,0x40,0x20,0x7C},

    /* 118 v */
    {0x1C,0x20,0x40,0x20,0x1C},

    /* 119 w */
    {0x3C,0x40,0x30,0x40,0x3C},

    /* 120 x */
    {0x44,0x28,0x10,0x28,0x44},

    /* 121 y */
    {0x0C,0x50,0x50,0x50,0x3C},

    /* 122 z */
    {0x44,0x64,0x54,0x4C,0x44},

    /* 123 { */
    {0x00,0x08,0x36,0x41,0x00},

    /* 124 | */
    {0x00,0x00,0x7F,0x00,0x00},

    /* 125 } */
    {0x00,0x41,0x36,0x08,0x00},

    /* 126 ~ */
    {0x08,0x04,0x08,0x10,0x08}
};

static HAL_StatusTypeDef SSD1306_WriteCommand(
    SSD1306_t *display,
    uint8_t command
)
{
    uint8_t data[2];

    data[0] = 0x00;
    data[1] = command;

    return HAL_I2C_Master_Transmit(
        display->i2c,
        display->address << 1,
        data,
        sizeof(data),
        HAL_MAX_DELAY
    );
}

HAL_StatusTypeDef SSD1306_Init(
    SSD1306_t *display,
    I2C_HandleTypeDef *i2c,
    uint8_t address
)
{
    display->i2c = i2c;
    display->address = address;

    display->cursorX = 0;
    display->cursorY = 0;

    HAL_Delay(100);

    /* Check that display responds */
    if (HAL_I2C_IsDeviceReady(
            i2c,
            address << 1,
            3,
            100
        ) != HAL_OK)
    {
        return HAL_ERROR;
    }

    /*
     * Display OFF
     */
    if (SSD1306_WriteCommand(display, 0xAE) != HAL_OK)
        return HAL_ERROR;

    /*
     * Memory addressing mode
     */
    SSD1306_WriteCommand(display, 0x20);

    /*
     * Horizontal addressing mode
     */
    SSD1306_WriteCommand(display, 0x00);

    /*
     * Start line = 0
     */
    SSD1306_WriteCommand(display, 0x40);

    /*
     * Segment remap
     */
    SSD1306_WriteCommand(display, 0xA1);

    /*
     * COM output scan direction
     */
    SSD1306_WriteCommand(display, 0xC8);

    /*
     * Multiplex ratio
     */
    SSD1306_WriteCommand(display, 0xA8);
    SSD1306_WriteCommand(display, 0x3F);

    /*
     * Display offset
     */
    SSD1306_WriteCommand(display, 0xD3);
    SSD1306_WriteCommand(display, 0x00);

    /*
     * Display clock
     */
    SSD1306_WriteCommand(display, 0xD5);
    SSD1306_WriteCommand(display, 0x80);

    /*
     * Pre-charge period
     */
    SSD1306_WriteCommand(display, 0xD9);
    SSD1306_WriteCommand(display, 0xF1);

    /*
     * COM pins hardware config
     */
    SSD1306_WriteCommand(display, 0xDA);
    SSD1306_WriteCommand(display, 0x12);

    /*
     * Contrast
     */
    SSD1306_WriteCommand(display, 0x81);
    SSD1306_WriteCommand(display, 0x7F);

    /*
     * VCOMH deselect
     */
    SSD1306_WriteCommand(display, 0xDB);
    SSD1306_WriteCommand(display, 0x40);

    /*
     * Entire display follows RAM
     */
    SSD1306_WriteCommand(display, 0xA4);

    /*
     * Normal display
     */
    SSD1306_WriteCommand(display, 0xA6);

    /*
     * Charge pump
     */
    SSD1306_WriteCommand(display, 0x8D);
    SSD1306_WriteCommand(display, 0x14);

    /*
     * Display ON
     */
    SSD1306_WriteCommand(display, 0xAF);


    SSD1306_Clear(display);

    return SSD1306_UpdateScreen(display);
}

void SSD1306_Clear(SSD1306_t *display)
{
    memset(
        display->buffer,
        0,
        sizeof(display->buffer)
    );

    display->cursorX = 0;
    display->cursorY = 0;
}

void SSD1306_DrawPixel(
    SSD1306_t *display,
    uint8_t x,
    uint8_t y,
    uint8_t color
)
{
    if (x >= SSD1306_WIDTH ||
        y >= SSD1306_HEIGHT)
    {
        return;
    }

    uint16_t index =
        x +
        (y / 8) * SSD1306_WIDTH;

    uint8_t bit =
        1 << (y % 8);

    if (color == SSD1306_COLOR_WHITE)
    {
        display->buffer[index] |= bit;
    }
    else
    {
        display->buffer[index] &= ~bit;
    }
}

void SSD1306_SetCursor(
    SSD1306_t *display,
    uint8_t x,
    uint8_t y
)
{
    display->cursorX = x;
    display->cursorY = y;
}

void SSD1306_WriteChar(
    SSD1306_t *display,
    char ch
)
{
    if (ch < 32 || ch > 126)
    {
        ch = '?';
    }

    /*
     * 5 pixels character width
     */
    for (uint8_t column = 0;
         column < 5;
         column++)
    {
        uint8_t columnData =
            font5x7[ch - 32][column];

        for (uint8_t row = 0;
             row < 7;
             row++)
        {
            if (columnData & (1 << row))
            {
                SSD1306_DrawPixel(
                    display,
                    display->cursorX + column,
                    display->cursorY + row,
                    SSD1306_COLOR_WHITE
                );
            }
        }
    }

    /*
     * Empty column between characters
     */
    display->cursorX += 6;


    /*
     * Simple line wrapping
     */
    if (display->cursorX + 5 >= SSD1306_WIDTH)
    {
        display->cursorX = 0;
        display->cursorY += 8;
    }
}

void SSD1306_WriteString(
    SSD1306_t *display,
    const char *str
)
{
    while (*str)
    {
        if (*str == '\n')
        {
            display->cursorX = 0;
            display->cursorY += 8;
        }
        else
        {
            SSD1306_WriteChar(
                display,
                *str
            );
        }

        str++;
    }
}

HAL_StatusTypeDef SSD1306_UpdateScreen(
    SSD1306_t *display
)
{
    uint8_t packet[SSD1306_WIDTH + 1];

    packet[0] = 0x40;

    for (uint8_t page = 0;
         page < SSD1306_PAGES;
         page++)
    {
        /*
         * Select page
         */
        SSD1306_WriteCommand(
            display,
            0xB0 + page
        );

        /*
         * Column low nibble
         */
        SSD1306_WriteCommand(
            display,
            0x00
        );

        /*
         * Column high nibble
         */
        SSD1306_WriteCommand(
            display,
            0x10
        );


        memcpy(
            &packet[1],
            &display->buffer[
                page * SSD1306_WIDTH
            ],
            SSD1306_WIDTH
        );


        HAL_StatusTypeDef status =
            HAL_I2C_Master_Transmit(
                display->i2c,
                display->address << 1,
                packet,
                sizeof(packet),
                HAL_MAX_DELAY
            );


        if (status != HAL_OK)
        {
            return status;
        }
    }

    return HAL_OK;
}
