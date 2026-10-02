#include "pressure_monitor.h"

#include "bmp280.h"
#include "ssd1306.h"

#include <stdbool.h>
#include <stdint.h>
#define BMP280_I2C_ADDRESS       0x76U
#define SSD1306_I2C_ADDRESS      0x3CU
#define PRESSURE_MIN_PA          30000U
#define PRESSURE_MAX_PA          120000U
#define MAX_SAMPLE_STEP_PA       2000U
#define STEP_CONFIRM_TOLERANCE   500U
#define STEP_CONFIRM_COUNT       3U
#define MAX_SENSOR_DIFFERENCE_PA 1000U

typedef enum {
  CHANNEL_OK = 0,
  CHANNEL_COMM_ERROR,
  CHANNEL_ID_ERROR,
  CHANNEL_RANGE_ERROR,
  CHANNEL_SPIKE_ERROR,
  CHANNEL_DATA_ERROR
} channel_status_t;

typedef struct {
  bmp280_t sensor;
  bmp280_status_t init_status;
  channel_status_t status;
  uint32_t history[3];
  uint8_t history_count;
  uint8_t history_index;
  uint32_t filtered_pa;
  uint32_t last_accepted_pa;
  uint32_t step_candidate_pa;
  uint8_t step_candidate_count;
  bool has_value;
} pressure_channel_t;

static pressure_channel_t i2c_channel;
static pressure_channel_t spi_channel;
static ssd1306_t display;
static I2C_HandleTypeDef *monitor_i2c;
static SPI_HandleTypeDef *monitor_spi;
static uint32_t last_update_ms;

static uint32_t absolute_difference(uint32_t left, uint32_t right)
{
  return (left >= right) ? (left - right) : (right - left);
}

static uint32_t median_value(const uint32_t *values, uint8_t count)
{
  if (count == 1U) {
    return values[0];
  }
  if (count == 2U) {
    return (values[0] + values[1]) / 2U;
  }
  uint32_t a = values[0];
  uint32_t b = values[1];
  uint32_t c = values[2];
  if (a > b) { uint32_t t = a; a = b; b = t; }
  if (b > c) { uint32_t t = b; b = c; c = t; }
  if (a > b) { uint32_t t = a; a = b; b = t; }
  return b;
}

static channel_status_t translate_error(bmp280_status_t status)
{
  if (status == BMP280_OK) {
    return CHANNEL_OK;
  }
  if (status == BMP280_ERROR_CHIP_ID) {
    return CHANNEL_ID_ERROR;
  }
  if (status == BMP280_ERROR_COMMUNICATION) {
    return CHANNEL_COMM_ERROR;
  }
  return CHANNEL_DATA_ERROR;
}

static void process_channel(pressure_channel_t *channel)
{
  if (!channel->sensor.initialized) {
    channel->status = translate_error(channel->init_status);
    return;
  }

  uint32_t raw_pa = 0U;
  bmp280_status_t read_status = bmp280_read_pressure_pa(&channel->sensor,
                                                        &raw_pa);
  if (read_status != BMP280_OK) {
    channel->status = translate_error(read_status);
    return;
  }
  if ((raw_pa < PRESSURE_MIN_PA) || (raw_pa > PRESSURE_MAX_PA)) {
    channel->status = CHANNEL_RANGE_ERROR;
    return;
  }

  if (channel->has_value &&
      (absolute_difference(raw_pa, channel->last_accepted_pa) >
       MAX_SAMPLE_STEP_PA)) {
    if ((channel->step_candidate_count == 0U) ||
        (absolute_difference(raw_pa, channel->step_candidate_pa) >
         STEP_CONFIRM_TOLERANCE)) {
      channel->step_candidate_pa = raw_pa;
      channel->step_candidate_count = 1U;
    } else {
      ++channel->step_candidate_count;
    }
    if (channel->step_candidate_count < STEP_CONFIRM_COUNT) {
      channel->status = CHANNEL_SPIKE_ERROR;
      return;
    }
  }

  channel->step_candidate_count = 0U;
  channel->last_accepted_pa = raw_pa;
  channel->history[channel->history_index] = raw_pa;
  channel->history_index = (uint8_t)((channel->history_index + 1U) % 3U);
  if (channel->history_count < 3U) {
    ++channel->history_count;
  }

  uint32_t median = median_value(channel->history, channel->history_count);
  if (!channel->has_value) {
    channel->filtered_pa = median;
    channel->has_value = true;
  } else {
    /* EMA alpha = 0.25 after a median-of-three pre-filter. */
    int32_t delta = (int32_t)median - (int32_t)channel->filtered_pa;
    channel->filtered_pa = (uint32_t)((int32_t)channel->filtered_pa +
                                      (delta / 4));
  }
  channel->status = CHANNEL_OK;
}

static char *append_text(char *output, const char *text)
{
  while (*text != '\0') {
    *output++ = *text++;
  }
  return output;
}

static char *append_uint(char *output, uint32_t value)
{
  char reverse[10];
  uint8_t count = 0U;
  do {
    reverse[count++] = (char)('0' + (value % 10U));
    value /= 10U;
  } while (value != 0U);
  while (count > 0U) {
    *output++ = reverse[--count];
  }
  return output;
}

static void format_pressure(char *output,
                            const char *name,
                            const pressure_channel_t *channel)
{
  char *cursor = append_text(output, name);
  cursor = append_text(cursor, ": ");
  if ((channel->status != CHANNEL_OK) || !channel->has_value) {
    cursor = append_text(cursor, "ERROR");
  } else {
    uint32_t whole_hpa = channel->filtered_pa / 100U;
    uint32_t fraction = channel->filtered_pa % 100U;
    cursor = append_uint(cursor, whole_hpa);
    *cursor++ = '.';
    *cursor++ = (char)('0' + (fraction / 10U));
    *cursor++ = (char)('0' + (fraction % 10U));
    cursor = append_text(cursor, " HPA");
  }
  *cursor = '\0';
}

static const char *channel_error_text(const char *name,
                                      channel_status_t status)
{
  if (status == CHANNEL_COMM_ERROR) {
    return (name[0] == 'I') ? "WARN: I2C COMM" : "WARN: SPI COMM";
  }
  if (status == CHANNEL_ID_ERROR) {
    return (name[0] == 'I') ? "WARN: I2C ID" : "WARN: SPI ID";
  }
  if (status == CHANNEL_RANGE_ERROR) {
    return (name[0] == 'I') ? "WARN: I2C RANGE" : "WARN: SPI RANGE";
  }
  if (status == CHANNEL_SPIKE_ERROR) {
    return (name[0] == 'I') ? "WARN: I2C SPIKE" : "WARN: SPI SPIKE";
  }
  return (name[0] == 'I') ? "WARN: I2C DATA" : "WARN: SPI DATA";
}

static const char *overall_status(void)
{
  if (i2c_channel.status != CHANNEL_OK) {
    return channel_error_text("I2C", i2c_channel.status);
  }
  if (spi_channel.status != CHANNEL_OK) {
    return channel_error_text("SPI", spi_channel.status);
  }
  if (absolute_difference(i2c_channel.filtered_pa,
                          spi_channel.filtered_pa) >
      MAX_SENSOR_DIFFERENCE_PA) {
    return "WARN: MISMATCH";
  }
  return "STATUS: OK";
}

static void render_display(void)
{
  if (!display.initialized) {
    return;
  }
  char line[22];
  ssd1306_clear(&display);
  ssd1306_write_text(&display, 13U, 0U, "PRESSURE MONITOR");
  format_pressure(line, "I2C", &i2c_channel);
  ssd1306_write_text(&display, 0U, 16U, line);
  format_pressure(line, "SPI", &spi_channel);
  ssd1306_write_text(&display, 0U, 28U, line);

  if ((i2c_channel.status == CHANNEL_OK) &&
      (spi_channel.status == CHANNEL_OK)) {
    char *cursor = append_text(line, "DIFF: ");
    uint32_t difference = absolute_difference(i2c_channel.filtered_pa,
                                              spi_channel.filtered_pa);
    cursor = append_uint(cursor, difference / 100U);
    *cursor++ = '.';
    *cursor++ = (char)('0' + ((difference % 100U) / 10U));
    *cursor++ = (char)('0' + (difference % 10U));
    cursor = append_text(cursor, " HPA");
    *cursor = '\0';
    ssd1306_write_text(&display, 0U, 40U, line);
  }
  ssd1306_write_text(&display, 0U, 54U, overall_status());
  if (!ssd1306_update(&display)) {
    display.initialized = false;
  }
}

void pressure_monitor_init(I2C_HandleTypeDef *i2c, SPI_HandleTypeDef *spi)
{
  monitor_i2c = i2c;
  monitor_spi = spi;
  i2c_channel = (pressure_channel_t){0};
  spi_channel = (pressure_channel_t){0};

  (void)ssd1306_init(&display, monitor_i2c, SSD1306_I2C_ADDRESS);
  i2c_channel.init_status = bmp280_init_i2c(&i2c_channel.sensor,
                                            monitor_i2c,
                                            BMP280_I2C_ADDRESS);
  spi_channel.init_status = bmp280_init_spi(&spi_channel.sensor,
                                            monitor_spi,
                                            GPIOA,
                                            GPIO_PIN_4);
  i2c_channel.status = translate_error(i2c_channel.init_status);
  spi_channel.status = translate_error(spi_channel.init_status);
  last_update_ms = HAL_GetTick() - 500U;
}

void pressure_monitor_update(void)
{
  uint32_t now = HAL_GetTick();
  if ((uint32_t)(now - last_update_ms) < 500U) {
    return;
  }
  last_update_ms = now;

  if (!i2c_channel.sensor.initialized) {
    i2c_channel.init_status = bmp280_init_i2c(&i2c_channel.sensor,
                                              monitor_i2c,
                                              BMP280_I2C_ADDRESS);
  }
  if (!spi_channel.sensor.initialized) {
    spi_channel.init_status = bmp280_init_spi(&spi_channel.sensor,
                                              monitor_spi,
                                              GPIOA,
                                              GPIO_PIN_4);
  }
  if (!display.initialized) {
    (void)ssd1306_init(&display, monitor_i2c, SSD1306_I2C_ADDRESS);
  }

  process_channel(&i2c_channel);
  process_channel(&spi_channel);
  render_display();

  bool warning = (i2c_channel.status != CHANNEL_OK) ||
                 (spi_channel.status != CHANNEL_OK) ||
                 (absolute_difference(i2c_channel.filtered_pa,
                                      spi_channel.filtered_pa) >
                  MAX_SENSOR_DIFFERENCE_PA);
  /* Blue Pill LED is active low: ON means invalid/warning. */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
                   warning ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
