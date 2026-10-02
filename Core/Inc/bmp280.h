#ifndef BMP280_H
#define BMP280_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
  BMP280_OK = 0,
  BMP280_ERROR_ARGUMENT,
  BMP280_ERROR_COMMUNICATION,
  BMP280_ERROR_CHIP_ID,
  BMP280_ERROR_CALIBRATION,
  BMP280_ERROR_DATA
} bmp280_status_t;

typedef enum {
  BMP280_BUS_I2C = 0,
  BMP280_BUS_SPI
} bmp280_bus_t;

typedef struct {
  uint16_t dig_t1;
  int16_t dig_t2;
  int16_t dig_t3;
  uint16_t dig_p1;
  int16_t dig_p2;
  int16_t dig_p3;
  int16_t dig_p4;
  int16_t dig_p5;
  int16_t dig_p6;
  int16_t dig_p7;
  int16_t dig_p8;
  int16_t dig_p9;
} bmp280_calibration_t;

typedef struct {
  bmp280_bus_t bus;
  I2C_HandleTypeDef *i2c;
  uint16_t i2c_address;
  SPI_HandleTypeDef *spi;
  GPIO_TypeDef *cs_port;
  uint16_t cs_pin;
  bmp280_calibration_t calibration;
  bool simulator_encoding;
  bool initialized;
} bmp280_t;

bmp280_status_t bmp280_init_i2c(bmp280_t *sensor,
                                I2C_HandleTypeDef *i2c,
                                uint8_t address_7bit);
bmp280_status_t bmp280_init_spi(bmp280_t *sensor,
                                SPI_HandleTypeDef *spi,
                                GPIO_TypeDef *cs_port,
                                uint16_t cs_pin);
bmp280_status_t bmp280_read_pressure_pa(bmp280_t *sensor,
                                        uint32_t *pressure_pa);

#endif
