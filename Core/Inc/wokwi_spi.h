#ifndef WOKWI_SPI_H
#define WOKWI_SPI_H
#include "stm32f1xx_hal.h"

/* Wokwi only: mode 0, MSB first, PA5 SCK / PA6 MISO / PA7 MOSI.
 * SensorTask owns CS (PA4) and the whole transaction. */
void wokwi_spi_init(void);
uint8_t wokwi_spi_exchange(uint8_t value);
#endif
