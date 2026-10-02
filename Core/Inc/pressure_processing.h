#ifndef PRESSURE_PROCESSING_H
#define PRESSURE_PROCESSING_H

#include "app_types.h"
#include <stdbool.h>

typedef struct {
  uint32_t values[APP_FILTER_SIZE];
  uint8_t count;
  uint8_t next;
} PressureFilter;

typedef struct {
  PressureFilter filter[APP_CHANNELS];
  uint32_t last_sequence;
  uint32_t last_sample_ms;
  uint32_t warnings;
  uint32_t last_warning_ms;
  bool have_sample;
} PressureProcessor;

/* Pure calculation: no HAL, RTOS, I/O or global mutable state. */
void pressure_process(PressureProcessor *state, const SensorRawData_t *raw,
                      uint32_t now_ms, DisplayData_t *out);
void pressure_mark_stale(DisplayData_t *data, uint32_t now_ms);
const char *pressure_status_text(PressureStatus status);

#endif
