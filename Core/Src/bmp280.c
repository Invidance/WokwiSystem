#include "bmp280.h"
#include "app_config.h"

#define BMP280_REG_ID       0xD0U
#define BMP280_REG_CONFIG   0xF5U
#define BMP280_REG_CONTROL  0xF4U
#define BMP280_REG_PRESSURE 0xF7U

static bmp280_status_t save_result(bmp280_t *sensor, HAL_StatusTypeDef status)
{
  sensor->last_status = status;
  sensor->last_error = sensor->bus == BMP280_BUS_I2C
      ? HAL_I2C_GetError(sensor->i2c) : HAL_SPI_GetError(sensor->spi);
  return status == HAL_OK ? BMP280_OK : BMP280_ERROR_COMMUNICATION;
}

static bmp280_status_t read_registers(bmp280_t *sensor, uint8_t reg,
                                      uint8_t *data, uint16_t length)
{
  if (sensor->bus == BMP280_BUS_I2C) {
    return save_result(sensor, HAL_I2C_Mem_Read(sensor->i2c,
        sensor->i2c_address, reg, I2C_MEMADD_SIZE_8BIT, data, length,
        APP_IO_TIMEOUT_MS));
  }

  uint8_t command = reg | 0x80U;
  uint8_t dummy = 0xFFU;
  HAL_GPIO_WritePin(sensor->cs_port, sensor->cs_pin, GPIO_PIN_RESET);
  HAL_StatusTypeDef status = HAL_SPI_Transmit(sensor->spi, &command, 1U,
                                              APP_IO_TIMEOUT_MS);
  /* One byte per exchange matches the custom chip's SPI callback. */
  for (uint16_t i = 0; status == HAL_OK && i < length; ++i) {
    status = HAL_SPI_TransmitReceive(sensor->spi, &dummy, &data[i], 1U,
                                     APP_IO_TIMEOUT_MS);
  }
  HAL_GPIO_WritePin(sensor->cs_port, sensor->cs_pin, GPIO_PIN_SET);
  return save_result(sensor, status);
}

static bmp280_status_t write_register(bmp280_t *sensor, uint8_t reg, uint8_t value)
{
  if (sensor->bus == BMP280_BUS_I2C) {
    return save_result(sensor, HAL_I2C_Mem_Write(sensor->i2c,
        sensor->i2c_address, reg, I2C_MEMADD_SIZE_8BIT, &value, 1U,
        APP_IO_TIMEOUT_MS));
  }
  uint8_t command = reg & 0x7FU;
  HAL_GPIO_WritePin(sensor->cs_port, sensor->cs_pin, GPIO_PIN_RESET);
  HAL_StatusTypeDef status = HAL_SPI_Transmit(sensor->spi, &command, 1U,
                                              APP_IO_TIMEOUT_MS);
  if (status == HAL_OK) {
    status = HAL_SPI_Transmit(sensor->spi, &value, 1U, APP_IO_TIMEOUT_MS);
  }
  HAL_GPIO_WritePin(sensor->cs_port, sensor->cs_pin, GPIO_PIN_SET);
  return save_result(sensor, status);
}

static bmp280_status_t initialize(bmp280_t *sensor)
{
  uint8_t id = 0;
  bmp280_status_t status = read_registers(sensor, BMP280_REG_ID, &id, 1U);
  if (status != BMP280_OK) {
    return status;
  }
  if (id != 0x58U) {
    return BMP280_ERROR_CHIP_ID;
  }
  status = write_register(sensor, BMP280_REG_CONFIG, 0x10U);
  if (status == BMP280_OK) {
    status = write_register(sensor, BMP280_REG_CONTROL, 0x27U);
  }
  sensor->initialized = status == BMP280_OK;
  return status;
}

bmp280_status_t bmp280_init_i2c(bmp280_t *sensor, I2C_HandleTypeDef *i2c,
                                uint8_t address_7bit)
{
  if (sensor == NULL || i2c == NULL) {
    return BMP280_ERROR_ARGUMENT;
  }
  *sensor = (bmp280_t){ .bus = BMP280_BUS_I2C, .i2c = i2c,
                        .i2c_address = (uint16_t)address_7bit << 1 };
  return initialize(sensor);
}

bmp280_status_t bmp280_init_spi(bmp280_t *sensor, SPI_HandleTypeDef *spi,
                                GPIO_TypeDef *cs_port, uint16_t cs_pin)
{
  if (sensor == NULL || spi == NULL || cs_port == NULL) {
    return BMP280_ERROR_ARGUMENT;
  }
  *sensor = (bmp280_t){ .bus = BMP280_BUS_SPI, .spi = spi,
                        .cs_port = cs_port, .cs_pin = cs_pin };
  HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
  return initialize(sensor);
}

bmp280_status_t bmp280_read_pressure_pa(bmp280_t *sensor, uint32_t *pressure_pa)
{
  if (sensor == NULL || pressure_pa == NULL || !sensor->initialized) {
    return BMP280_ERROR_ARGUMENT;
  }
  uint8_t raw[3];
  bmp280_status_t status = read_registers(sensor, BMP280_REG_PRESSURE, raw, 3U);
  if (status == BMP280_OK) {
    uint32_t raw20 = ((uint32_t)raw[0] << 12) |
                    ((uint32_t)raw[1] << 4) | (raw[2] >> 4);
    *pressure_pa = raw20 >> 3; /* Explicit Wokwi encoding: raw20 = Pa * 8. */
  }
  return status;
}
