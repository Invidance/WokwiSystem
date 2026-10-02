#include "pressure_processing.h"

static uint32_t distance(uint32_t a, uint32_t b)
{
  return a > b ? a - b : b - a;
}

static uint32_t median(const PressureFilter *filter)
{
  uint32_t sorted[APP_FILTER_SIZE];
  for (unsigned i = 0; i < APP_FILTER_SIZE; ++i) {
    sorted[i] = filter->values[i];
    for (unsigned j = i; j > 0 && sorted[j] < sorted[j - 1]; --j) {
      uint32_t temporary = sorted[j];
      sorted[j] = sorted[j - 1];
      sorted[j - 1] = temporary;
    }
  }
  return sorted[APP_FILTER_SIZE / 2U];
}

void pressure_mark_stale(DisplayData_t *data, uint32_t now_ms)
{
  if ((uint32_t)(now_ms - data->timestamp_ms) >= APP_STALE_MS) {
    for (unsigned i = 0; i < APP_CHANNELS; ++i) {
      data->channel[i].status = PRESSURE_STALE;
      data->warnings |= PRESSURE_WARNING(i, PRESSURE_STALE);
    }
    data->difference_pa = 0;
  }
}

void pressure_process(PressureProcessor *state, const SensorRawData_t *raw,
                      uint32_t now_ms, DisplayData_t *out)
{
  *out = (DisplayData_t){ .timestamp_ms = raw->timestamp_ms };
  bool gap = state->have_sample &&
      ((uint32_t)(raw->sequence - state->last_sequence) != 1U ||
       (uint32_t)(raw->timestamp_ms - state->last_sample_ms) >=
           2U * APP_SENSOR_PERIOD_MS);
  state->have_sample = true;
  state->last_sequence = raw->sequence;
  state->last_sample_ms = raw->timestamp_ms;

  uint32_t active_warnings = 0;
  for (unsigned i = 0; i < APP_CHANNELS; ++i) {
    PressureFilter *filter = &state->filter[i];
    const SensorReading *input = &raw->channel[i];
    DisplayReading *result = &out->channel[i];
    result->status = input->status;
    result->hal_status = input->hal_status;
    result->hal_error = input->hal_error;
    if (gap) {
      *filter = (PressureFilter){0};
    }
    if ((uint32_t)(now_ms - raw->timestamp_ms) >= APP_STALE_MS) {
      result->status = PRESSURE_STALE;
    } else if (result->status == PRESSURE_OK &&
               (input->pressure_pa < APP_PRESSURE_MIN_PA ||
                input->pressure_pa > APP_PRESSURE_MAX_PA)) {
      result->status = PRESSURE_RANGE;
    }
    if (result->status != PRESSURE_OK) {
      *filter = (PressureFilter){0};
    } else {
      filter->values[filter->next] = input->pressure_pa;
      filter->next = (uint8_t)((filter->next + 1U) % APP_FILTER_SIZE);
      if (filter->count < APP_FILTER_SIZE) {
        ++filter->count;
      }
      if (filter->count < APP_FILTER_SIZE) {
        result->status = PRESSURE_WAIT;
      } else {
        result->pressure_pa = median(filter);
        if (distance(input->pressure_pa, result->pressure_pa) > APP_SPIKE_PA) {
          /* Keep it in the window: a persistent change is accepted naturally. */
          result->status = PRESSURE_SPIKE;
        }
      }
    }
    if (result->status != PRESSURE_OK && result->status != PRESSURE_WAIT) {
      active_warnings |= PRESSURE_WARNING(i, result->status);
    }
  }
  if (out->channel[0].status == PRESSURE_OK &&
      out->channel[1].status == PRESSURE_OK) {
    out->difference_pa = distance(out->channel[0].pressure_pa,
                                  out->channel[1].pressure_pa);
    if (out->difference_pa > APP_MISMATCH_PA) {
      active_warnings |= PRESSURE_WARNING_MISMATCH;
    }
  }
  if ((uint32_t)(now_ms - state->last_warning_ms) >= APP_WARNING_HOLD_MS) {
    state->warnings = 0;
  }
  if (active_warnings != 0U) {
    state->warnings |= active_warnings;
    state->last_warning_ms = now_ms;
  }
  out->warnings = state->warnings;
}

const char *pressure_status_text(PressureStatus status)
{
  switch (status) {
    case PRESSURE_WAIT:  return "WAIT";
    case PRESSURE_OK:    return "OK";
    case PRESSURE_COMM:  return "COMM";
    case PRESSURE_ID:    return "ID";
    case PRESSURE_RANGE: return "RANGE";
    case PRESSURE_SPIKE: return "SPIKE";
    case PRESSURE_STALE: return "STALE";
    default:            return "UNKNOWN";
  }
}
