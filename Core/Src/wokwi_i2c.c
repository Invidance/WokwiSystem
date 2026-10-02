#include "wokwi_i2c.h"

#define SOFT_I2C_PORT       GPIOB
#define SOFT_I2C_SCL        GPIO_PIN_8
#define SOFT_I2C_SDA        GPIO_PIN_9
#define SOFT_I2C_STRETCH_MAX 200U

static bool configured;
static uint32_t last_error;

/* About 5 us at the Wokwi reset clock of 8 MHz. Exact timing is unimportant
 * for these simulated standard-mode devices, but both phases must be visible. */
static inline void half_period(void)
{
  __asm volatile(
      "movs r3, #12\n"
      "1: subs r3, r3, #1\n"
      "bne 1b\n"
      ::: "r3", "cc", "memory");
}

static inline void scl_low(void)     { SOFT_I2C_PORT->BRR = SOFT_I2C_SCL; }
static inline void scl_release(void) { SOFT_I2C_PORT->BSRR = SOFT_I2C_SCL; }
static inline void sda_low(void)     { SOFT_I2C_PORT->BRR = SOFT_I2C_SDA; }
static inline void sda_release(void) { SOFT_I2C_PORT->BSRR = SOFT_I2C_SDA; }
static inline bool scl_is_high(void) { return (SOFT_I2C_PORT->IDR & SOFT_I2C_SCL) != 0U; }
static inline bool sda_is_high(void) { return (SOFT_I2C_PORT->IDR & SOFT_I2C_SDA) != 0U; }

static bool wait_scl_high(void)
{
  for (uint32_t i = 0U; i < SOFT_I2C_STRETCH_MAX; ++i) {
    if (scl_is_high()) { return true; }
    half_period();
  }
  last_error = HAL_I2C_ERROR_TIMEOUT;
  return false;
}

static void configure_pins(void)
{
  if (configured) { return; }

  __HAL_RCC_GPIOB_CLK_ENABLE();

  /* PB8/PB9 are plain GPIO in the Wokwi build.  Do not use the simulated
   * I2C1 pins PB6/PB7: that peripheral model may keep SCL low. */
  scl_release();
  sda_release();

  GPIO_InitTypeDef gpio = {0};
  gpio.Pin = SOFT_I2C_SCL | SOFT_I2C_SDA;
  gpio.Mode = GPIO_MODE_OUTPUT_OD;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(SOFT_I2C_PORT, &gpio);
  scl_release();
  sda_release();
  configured = true;
  half_period();
}

static bool start_condition(void)
{
  sda_release();
  scl_release();
  if (!wait_scl_high()) { return false; }
  if (!sda_is_high()) {
    last_error = HAL_I2C_ERROR_TIMEOUT;
    return false;
  }
  half_period();
  sda_low();
  half_period();
  scl_low();
  return true;
}

static void stop_condition(void)
{
  scl_low();
  sda_low();
  half_period();
  scl_release();
  (void)wait_scl_high();
  half_period();
  sda_release();
  half_period();
}

static bool write_byte(uint8_t value)
{
  for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1U) {
    scl_low();
    if ((value & mask) != 0U) { sda_release(); } else { sda_low(); }
    half_period();
    scl_release();
    if (!wait_scl_high()) { return false; }
    half_period();
  }

  scl_low();
  sda_release();
  half_period();
  scl_release();
  if (!wait_scl_high()) { return false; }
  bool acknowledged = !sda_is_high();
  half_period();
  scl_low();
  if (!acknowledged) { last_error = HAL_I2C_ERROR_AF; }
  return acknowledged;
}

static bool read_byte(uint8_t *value, bool acknowledge)
{
  uint8_t result = 0U;
  sda_release();
  for (uint8_t i = 0U; i < 8U; ++i) {
    scl_low();
    half_period();
    scl_release();
    if (!wait_scl_high()) { return false; }
    result = (uint8_t)((result << 1U) | (sda_is_high() ? 1U : 0U));
    half_period();
  }

  scl_low();
  if (acknowledge) { sda_low(); } else { sda_release(); }
  half_period();
  scl_release();
  if (!wait_scl_high()) { return false; }
  half_period();
  scl_low();
  sda_release();
  *value = result;
  return true;
}

HAL_StatusTypeDef wokwi_i2c_recover(void)
{
  configure_pins();
  last_error = HAL_I2C_ERROR_NONE;
  sda_release();
  for (uint8_t i = 0U; i < 9U && !sda_is_high(); ++i) {
    scl_low();
    half_period();
    scl_release();
    if (!wait_scl_high()) { return HAL_TIMEOUT; }
    half_period();
  }
  stop_condition();
  if (!scl_is_high() || !sda_is_high()) {
    last_error = HAL_I2C_ERROR_TIMEOUT;
    return HAL_TIMEOUT;
  }
  return HAL_OK;
}

HAL_StatusTypeDef wokwi_i2c_init(void)
{
  bool first_initialization = !configured;
  configure_pins();
  return first_initialization ? wokwi_i2c_recover() : HAL_OK;
}

HAL_StatusTypeDef wokwi_i2c_probe(uint8_t address_7bit)
{
  last_error = HAL_I2C_ERROR_NONE;
  if (wokwi_i2c_init() != HAL_OK || !start_condition()) { return HAL_TIMEOUT; }
  bool ok = write_byte((uint8_t)(address_7bit << 1U));
  stop_condition();
  return ok ? HAL_OK : (last_error == HAL_I2C_ERROR_AF ? HAL_ERROR : HAL_TIMEOUT);
}

HAL_StatusTypeDef wokwi_i2c_mem_write(uint8_t address_7bit, uint8_t reg,
                                      const uint8_t *data, uint16_t length)
{
  if ((data == NULL) && (length != 0U)) { return HAL_ERROR; }
  last_error = HAL_I2C_ERROR_NONE;
  if (wokwi_i2c_init() != HAL_OK || !start_condition()) { return HAL_TIMEOUT; }

  bool ok = write_byte((uint8_t)(address_7bit << 1U)) && write_byte(reg);
  for (uint16_t i = 0U; ok && i < length; ++i) { ok = write_byte(data[i]); }
  stop_condition();
  return ok ? HAL_OK : (last_error == HAL_I2C_ERROR_AF ? HAL_ERROR : HAL_TIMEOUT);
}

HAL_StatusTypeDef wokwi_i2c_mem_read(uint8_t address_7bit, uint8_t reg,
                                     uint8_t *data, uint16_t length)
{
  if ((data == NULL) || (length == 0U)) { return HAL_ERROR; }
  last_error = HAL_I2C_ERROR_NONE;
  if (wokwi_i2c_init() != HAL_OK || !start_condition()) { return HAL_TIMEOUT; }

  bool ok = write_byte((uint8_t)(address_7bit << 1U)) && write_byte(reg);
  if (ok) { ok = start_condition(); }
  if (ok) { ok = write_byte((uint8_t)((address_7bit << 1U) | 1U)); }
  for (uint16_t i = 0U; ok && i < length; ++i) {
    ok = read_byte(&data[i], i + 1U < length);
  }
  stop_condition();
  return ok ? HAL_OK : (last_error == HAL_I2C_ERROR_AF ? HAL_ERROR : HAL_TIMEOUT);
}

uint32_t wokwi_i2c_last_error(void)
{
  return last_error;
}
