#ifndef PRESSURE_MONITOR_H
#define PRESSURE_MONITOR_H

#include "stm32f1xx_hal.h"

void pressure_monitor_init(I2C_HandleTypeDef *i2c, SPI_HandleTypeDef *spi);
void pressure_monitor_update(void);

#endif
