#ifndef BMP280_H
#define BMP280_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* This driver targets chips/bmp280.chip.c, NOT physical BMP280 calibration. */
typedef enum {
  BMP280_OK = 0,
  BMP280_ERROR_ARGUMENT,
  BMP280_ERROR_COMMUNICATION,
  BMP280_ERROR_CHIP_ID
} bmp280_status_t;

typedef enum { BMP280_BUS_I2C = 0, BMP280_BUS_SPI } bmp280_bus_t;

typedef struct {
  bmp280_bus_t bus;
  I2C_HandleTypeDef *i2c;
  uint16_t i2c_address;
  SPI_HandleTypeDef *spi;
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
  bool initialized;
  HAL_StatusTypeDef last_status;
  uint32_t last_error;
} bmp280_t;

bmp280_status_t bmp280_init_i2c(bmp280_t *sensor, I2C_HandleTypeDef *i2c,
                                uint8_t address_7bit);
bmp280_status_t bmp280_init_spi(bmp280_t *sensor, SPI_HandleTypeDef *spi,
                                GPIO_TypeDef *cs_port, uint16_t cs_pin);
bmp280_status_t bmp280_read_pressure_pa(bmp280_t *sensor, uint32_t *pressure_pa);

#endif
