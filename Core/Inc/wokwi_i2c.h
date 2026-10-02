#ifndef WOKWI_I2C_H
#define WOKWI_I2C_H

#include "stm32f1xx_hal.h"
#include <stdbool.h>

/*
 * GPIO I2C master used only by the Wokwi build.  It bypasses the simulated
 * STM32 I2C1 state machine while keeping the same PB6/PB7 bus and devices.
 */
HAL_StatusTypeDef wokwi_i2c_init(void);
HAL_StatusTypeDef wokwi_i2c_recover(void);
HAL_StatusTypeDef wokwi_i2c_probe(uint8_t address_7bit);
HAL_StatusTypeDef wokwi_i2c_mem_write(uint8_t address_7bit, uint8_t reg,
                                      const uint8_t *data, uint16_t length);
HAL_StatusTypeDef wokwi_i2c_mem_read(uint8_t address_7bit, uint8_t reg,
                                     uint8_t *data, uint16_t length);
uint32_t wokwi_i2c_last_error(void);

#endif
