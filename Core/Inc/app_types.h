#ifndef APP_TYPES_H
#define APP_TYPES_H

#include "app_config.h"
#include <stdint.h>

typedef enum {
  PRESSURE_WAIT = 0,
  PRESSURE_OK,
  PRESSURE_COMM,
  PRESSURE_ID,
  PRESSURE_RANGE,
  PRESSURE_SPIKE,
  PRESSURE_STALE
} PressureStatus;

/* Unfiltered Pa, NOT the sensor's packed ADC register value. */
typedef struct {
  uint32_t pressure_pa;
  PressureStatus status;
  uint32_t hal_status;
  uint32_t hal_error;
} SensorReading;

typedef struct {
  uint32_t sequence;
  uint32_t timestamp_ms;
  SensorReading channel[APP_CHANNELS];
} SensorRawData_t;

typedef struct {
  uint32_t pressure_pa;
  PressureStatus status;
  uint32_t hal_status;
  uint32_t hal_error;
} DisplayReading;

/* Each channel has eight warning bits, indexed by PressureStatus. */
#define PRESSURE_WARNING(channel, status) (1UL << ((channel) * 8U + (status)))
#define PRESSURE_WARNING_MISMATCH (1UL << 16U)

typedef struct {
  uint32_t timestamp_ms;
  DisplayReading channel[APP_CHANNELS];
  uint32_t difference_pa; /* meaningful only when both statuses are OK */
  uint32_t warnings;      /* includes short-lived warnings held for 2 seconds */
} DisplayData_t;

#endif
