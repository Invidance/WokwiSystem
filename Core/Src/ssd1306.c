#include "ssd1306.h"
#include "app_config.h"

#define SSD1306_COMMAND    0x00U
#define SSD1306_DATA       0x40U

static void clear_bytes(uint8_t *data, uint16_t length)
{
  while (length-- > 0U) {
    *data++ = 0U;
  }
}

static void copy_bytes(uint8_t *destination,
                       const uint8_t *source,
                       uint16_t length)
{
  while (length-- > 0U) {
    *destination++ = *source++;
  }
}

static bool save_status(ssd1306_t *display, HAL_StatusTypeDef status,
                        ssd1306_phase_t phase, uint8_t command, uint8_t page)
{
  display->last_status = status;
  display->last_i2c_error = HAL_I2C_GetError(display->i2c);
  if (status != HAL_OK) {
    display->initialized = false;
    display->failure.phase = phase;
    display->failure.command = command;
    display->failure.page = page;
    display->failure.status = status;
    display->failure.i2c_error = display->last_i2c_error;
    display->failure.hal_tick = HAL_GetTick();
    ++display->failure.count;
  }
  return status == HAL_OK;
}

static bool send_command(ssd1306_t *display, uint8_t value,
                         ssd1306_phase_t phase, uint8_t page)
{
  HAL_StatusTypeDef status = HAL_I2C_Mem_Write(display->i2c, display->address,
      SSD1306_COMMAND, I2C_MEMADD_SIZE_8BIT, &value, 1U, APP_IO_TIMEOUT_MS);
  return save_status(display, status, phase, value, page);
}

static void glyph(char character, uint8_t output[5])
{
  clear_bytes(output, 5U);
  switch (character) {
    case '0': { const uint8_t g[5] = {0x3E,0x51,0x49,0x45,0x3E}; copy_bytes(output,g,5); break; }
    case '1': { const uint8_t g[5] = {0x00,0x42,0x7F,0x40,0x00}; copy_bytes(output,g,5); break; }
    case '2': { const uint8_t g[5] = {0x42,0x61,0x51,0x49,0x46}; copy_bytes(output,g,5); break; }
    case '3': { const uint8_t g[5] = {0x21,0x41,0x45,0x4B,0x31}; copy_bytes(output,g,5); break; }
    case '4': { const uint8_t g[5] = {0x18,0x14,0x12,0x7F,0x10}; copy_bytes(output,g,5); break; }
    case '5': { const uint8_t g[5] = {0x27,0x45,0x45,0x45,0x39}; copy_bytes(output,g,5); break; }
    case '6': { const uint8_t g[5] = {0x3C,0x4A,0x49,0x49,0x30}; copy_bytes(output,g,5); break; }
    case '7': { const uint8_t g[5] = {0x01,0x71,0x09,0x05,0x03}; copy_bytes(output,g,5); break; }
    case '8': { const uint8_t g[5] = {0x36,0x49,0x49,0x49,0x36}; copy_bytes(output,g,5); break; }
    case '9': { const uint8_t g[5] = {0x06,0x49,0x49,0x29,0x1E}; copy_bytes(output,g,5); break; }
    case 'A': { const uint8_t g[5] = {0x7E,0x11,0x11,0x11,0x7E}; copy_bytes(output,g,5); break; }
    case 'B': { const uint8_t g[5] = {0x7F,0x49,0x49,0x49,0x36}; copy_bytes(output,g,5); break; }
    case 'C': { const uint8_t g[5] = {0x3E,0x41,0x41,0x41,0x22}; copy_bytes(output,g,5); break; }
    case 'D': { const uint8_t g[5] = {0x7F,0x41,0x41,0x22,0x1C}; copy_bytes(output,g,5); break; }
    case 'E': { const uint8_t g[5] = {0x7F,0x49,0x49,0x49,0x41}; copy_bytes(output,g,5); break; }
    case 'F': { const uint8_t g[5] = {0x7F,0x09,0x09,0x09,0x01}; copy_bytes(output,g,5); break; }
    case 'G': { const uint8_t g[5] = {0x3E,0x41,0x49,0x49,0x7A}; copy_bytes(output,g,5); break; }
    case 'H': { const uint8_t g[5] = {0x7F,0x08,0x08,0x08,0x7F}; copy_bytes(output,g,5); break; }
    case 'I': { const uint8_t g[5] = {0x00,0x41,0x7F,0x41,0x00}; copy_bytes(output,g,5); break; }
    case 'J': { const uint8_t g[5] = {0x20,0x40,0x41,0x3F,0x01}; copy_bytes(output,g,5); break; }
    case 'K': { const uint8_t g[5] = {0x7F,0x08,0x14,0x22,0x41}; copy_bytes(output,g,5); break; }
    case 'L': { const uint8_t g[5] = {0x7F,0x40,0x40,0x40,0x40}; copy_bytes(output,g,5); break; }
    case 'M': { const uint8_t g[5] = {0x7F,0x02,0x0C,0x02,0x7F}; copy_bytes(output,g,5); break; }
    case 'N': { const uint8_t g[5] = {0x7F,0x04,0x08,0x10,0x7F}; copy_bytes(output,g,5); break; }
    case 'O': { const uint8_t g[5] = {0x3E,0x41,0x41,0x41,0x3E}; copy_bytes(output,g,5); break; }
    case 'P': { const uint8_t g[5] = {0x7F,0x09,0x09,0x09,0x06}; copy_bytes(output,g,5); break; }
    case 'Q': { const uint8_t g[5] = {0x3E,0x41,0x51,0x21,0x5E}; copy_bytes(output,g,5); break; }
    case 'R': { const uint8_t g[5] = {0x7F,0x09,0x19,0x29,0x46}; copy_bytes(output,g,5); break; }
    case 'S': { const uint8_t g[5] = {0x46,0x49,0x49,0x49,0x31}; copy_bytes(output,g,5); break; }
    case 'T': { const uint8_t g[5] = {0x01,0x01,0x7F,0x01,0x01}; copy_bytes(output,g,5); break; }
    case 'U': { const uint8_t g[5] = {0x3F,0x40,0x40,0x40,0x3F}; copy_bytes(output,g,5); break; }
    case 'V': { const uint8_t g[5] = {0x1F,0x20,0x40,0x20,0x1F}; copy_bytes(output,g,5); break; }
    case 'W': { const uint8_t g[5] = {0x3F,0x40,0x38,0x40,0x3F}; copy_bytes(output,g,5); break; }
    case 'X': { const uint8_t g[5] = {0x63,0x14,0x08,0x14,0x63}; copy_bytes(output,g,5); break; }
    case 'Y': { const uint8_t g[5] = {0x07,0x08,0x70,0x08,0x07}; copy_bytes(output,g,5); break; }
    case 'Z': { const uint8_t g[5] = {0x61,0x51,0x49,0x45,0x43}; copy_bytes(output,g,5); break; }
    case ':': { const uint8_t g[5] = {0x00,0x36,0x36,0x00,0x00}; copy_bytes(output,g,5); break; }
    case '.': { const uint8_t g[5] = {0x00,0x60,0x60,0x00,0x00}; copy_bytes(output,g,5); break; }
    case '-': { const uint8_t g[5] = {0x08,0x08,0x08,0x08,0x08}; copy_bytes(output,g,5); break; }
    case '/': { const uint8_t g[5] = {0x20,0x10,0x08,0x04,0x02}; copy_bytes(output,g,5); break; }
    default: break;
  }
}

bool ssd1306_init(ssd1306_t *display,
                  I2C_HandleTypeDef *i2c,
                  uint8_t address_7bit)
{
  if ((display == NULL) || (i2c == NULL)) {
    return false;
  }
  display->initialized = false;
  display->i2c = i2c;
  display->address = (uint16_t)address_7bit << 1;

  HAL_StatusTypeDef status = HAL_I2C_IsDeviceReady(
      display->i2c, display->address, 1U, APP_IO_TIMEOUT_MS);
  if (!save_status(display, status, SSD1306_PROBE, 0U, 0U)) {
    return false;
  }

  /* Horizontal addressing is also used when writing the framebuffer. */
  static const uint8_t init_commands[] = {
    0xAE,             /* display off */
    0x20, 0x00,       /* horizontal addressing mode */
    0x40,             /* start line 0 */
    0xA1,             /* segment remap */
    0xC8,             /* COM scan direction */
    0xA8, 0x3F,       /* multiplex ratio */
    0xD3, 0x00,       /* display offset */
    0xD5, 0x80,       /* display clock */
    0xD9, 0xF1,       /* pre-charge period */
    0xDA, 0x12,       /* COM pins configuration */
    0x81, 0x7F,       /* contrast */
    0xDB, 0x40,       /* VCOMH deselect */
    0xA4,             /* display follows RAM */
    0xA6,             /* normal display */
    0x8D, 0x14,       /* charge pump */
    0xAF              /* display on */
  };
  for (uint16_t i = 0U; i < sizeof(init_commands); ++i) {
    if (!send_command(display, init_commands[i], SSD1306_INIT_COMMAND, 0U)) {
      return false;
    }
  }
  ssd1306_clear(display);
  display->initialized = true;
  /* The monitor draws and transmits the first frame after initialization. */
  return true;
}

void ssd1306_clear(ssd1306_t *display)
{
  if (display != NULL) {
    clear_bytes(display->buffer, sizeof(display->buffer));
  }
}

static void draw_pixel(ssd1306_t *display, uint8_t x, uint8_t y)
{
  if ((x >= SSD1306_WIDTH) || (y >= SSD1306_HEIGHT)) {
    return;
  }
  display->buffer[x + ((uint16_t)(y / 8U) * SSD1306_WIDTH)] |=
      (uint8_t)(1U << (y & 7U));
}

void ssd1306_write_text(ssd1306_t *display,
                        uint8_t x,
                        uint8_t y,
                        const char *text)
{
  if ((display == NULL) || (text == NULL)) {
    return;
  }
  while ((*text != '\0') && (x <= (SSD1306_WIDTH - 6U))) {
    uint8_t columns[5];
    glyph(*text, columns);
    for (uint8_t column = 0U; column < 5U; ++column) {
      for (uint8_t row = 0U; row < 7U; ++row) {
        if ((columns[column] & (1U << row)) != 0U) {
          draw_pixel(display, (uint8_t)(x + column), (uint8_t)(y + row));
        }
      }
    }
    x = (uint8_t)(x + 6U);
    ++text;
  }
}

bool ssd1306_update_page(ssd1306_t *display, uint8_t page)
{
  if ((display == NULL) || (display->i2c == NULL) || !display->initialized ||
      page >= SSD1306_PAGES) {
    return false;
  }

  /*
   * Horizontal addressing, restricted to one page. No page-mode commands.
   * Send 128 bytes in one transaction instead of 128 separate transactions.
   */
  const uint8_t address_window[] = {
    0x21U, 0x00U, 0x7FU, /* columns 0..127 */
    0x22U, page, page
  };
  for (uint16_t i = 0U; i < sizeof(address_window); ++i) {
    if (!send_command(display, address_window[i], SSD1306_WINDOW_COMMAND, page)) {
      return false;
    }
  }

  HAL_StatusTypeDef status = HAL_I2C_Mem_Write(display->i2c, display->address,
      SSD1306_DATA, I2C_MEMADD_SIZE_8BIT, &display->buffer[page * SSD1306_WIDTH],
      SSD1306_WIDTH, APP_IO_TIMEOUT_MS);
  return save_status(display, status, SSD1306_PAGE_DATA, 0U, page);
}
