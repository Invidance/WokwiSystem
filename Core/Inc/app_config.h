#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Wokwi custom BMP280 format, not the physical Bosch calibration protocol. */
#define APP_SENSOR_PERIOD_MS       100U
#define APP_DISPLAY_PERIOD_MS      500U
#define APP_RETRY_MS              1000U
#define APP_STALE_MS              1000U
#define APP_WARNING_HOLD_MS       2000U
#define APP_IO_TIMEOUT_MS           50U
#define APP_MUTEX_TIMEOUT_MS       100U
#define APP_OLED_BOOT_MS           100U
#define APP_FILTER_SIZE              5U
#define APP_PRESSURE_MIN_PA      30000U
#define APP_PRESSURE_MAX_PA     120000U
#define APP_SPIKE_PA              2000U
#define APP_MISMATCH_PA           1000U
#define APP_BMP280_ADDRESS        0x76U
#define APP_OLED_ADDRESS          0x3CU
#define APP_CHANNELS                 2U
#define APP_I2C_CHANNEL              0U
#define APP_SPI_CHANNEL              1U

#endif
