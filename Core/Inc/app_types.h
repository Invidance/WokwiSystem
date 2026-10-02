#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

typedef struct
{
    float i2cPressure;
    float spiPressure;

    uint8_t i2cStatus;
    uint8_t spiStatus;

} SensorRawData_t;

typedef struct
{
    float i2cPressure;
    float spiPressure;

    float filteredI2C;
    float filteredSPI;

    uint8_t i2cValid;
    uint8_t spiValid;
    uint8_t mismatch;

} DisplayData_t;

#endif