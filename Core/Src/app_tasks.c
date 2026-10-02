#include "app_tasks.h"
#include "app_config.h"
#include "app_types.h"
#include "bmp280.h"
#include "pressure_processing.h"
#include "ssd1306.h"
#if defined(WOKWI_ENABLED)
#include "wokwi_i2c.h"
#endif
#include "cmsis_os.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

/* Created by CubeMX in main.c. No duplicate RTOS objects here. */
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart1;
extern osMutexId_t i2cmutexHandle;
extern osMessageQueueId_t RawQueueHandle, DisplayQueueHandle;
extern osThreadId_t SensorTaskHandle, ProcessingTaskHandle, DisplayTaskHandle;

volatile AppDebug app_debug;
volatile AppFault app_fault;
const char * volatile app_fault_task;
static ssd1306_t oled; /* DisplayTask is the sole owner. */

#if defined(WOKWI_ENABLED)
#define APP_RTOS_PORT_NAME "WOKWI-DIRECT"
#define APP_I2C_TRANSPORT_NAME "SOFT"
#define APP_SPI_TRANSPORT_NAME "SOFT"
extern volatile uint32_t wokwi_port_idle_calls;
extern volatile uint32_t wokwi_port_switch_count;
#else
#define APP_RTOS_PORT_NAME "ARM-CM3"
#define APP_I2C_TRANSPORT_NAME "HAL"
#define APP_SPI_TRANSPORT_NAME "HAL"
#endif

_Noreturn void app_panic(AppFault fault)
{
  app_fault = fault;
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
  __disable_irq();
  for (;;) { /* Inspect app_fault and app_fault_task with GDB. */ }
}

void vApplicationMallocFailedHook(void)
{
  app_panic(APP_FAULT_HEAP);
}

void vApplicationStackOverflowHook(TaskHandle_t task, signed char *name)
{
  (void)task;
  app_fault_task = (const char *)name;
  app_panic(APP_FAULT_STACK); /* Never log or wait from a failure hook. */
}

/* RTOS ticks are explicitly configured to 1 ms in FreeRTOSConfig.h. */
static uint32_t now_ms(void) { return osKernelGetTickCount(); }

static void wait_period(uint32_t *deadline, uint32_t period_ms)
{
  *deadline += period_ms;
  uint32_t now = now_ms();
  if ((int32_t)(*deadline - now) <= 0) {
    *deadline = now + period_ms; /* Skip missed periods; always yield. */
  }
  if (osDelayUntil(*deadline) != osOK) {
    /* A preemption can cross the deadline between the check and the call. */
    osDelay(1U);
  }
}

static bool lock_i2c(void)
{
  if (osMutexAcquire(i2cmutexHandle, APP_MUTEX_TIMEOUT_MS) == osOK) {
    return true;
  }
  /* Both callers may update this diagnostic counter. */
  taskENTER_CRITICAL();
  ++app_debug.mutex_timeouts;
  taskEXIT_CRITICAL();
  return false;
}

/* Called ONLY while holding i2cmutex, AFTER the driver saves its error. */
static void recover_i2c(HAL_StatusTypeDef status, uint32_t error)
{
  static uint32_t last_attempt;
  static bool attempted;
  bool stuck = status == HAL_BUSY || status == HAL_TIMEOUT ||
               (error & HAL_I2C_ERROR_TIMEOUT) != 0U;
  if (!stuck || (attempted &&
      (uint32_t)(now_ms() - last_attempt) < APP_RETRY_MS)) {
    return; /* In particular, a plain NACK does not reset the shared bus. */
  }
  attempted = true;
  last_attempt = now_ms();
#if defined(WOKWI_ENABLED)
  app_debug.recovery_status = wokwi_i2c_recover();
#else
  SET_BIT(hi2c1.Instance->CR1, I2C_CR1_STOP);
  (void)HAL_I2C_DeInit(&hi2c1);
  __HAL_RCC_I2C1_FORCE_RESET();
  __HAL_RCC_I2C1_RELEASE_RESET();
  app_debug.recovery_status = HAL_I2C_Init(&hi2c1);
#endif
  ++app_debug.i2c_recoveries;
}

static PressureStatus sensor_status(bmp280_status_t status)
{
  if (status == BMP280_OK) { return PRESSURE_OK; }
  if (status == BMP280_ERROR_CHIP_ID) { return PRESSURE_ID; }
  return PRESSURE_COMM;
}

/* Called by SensorTask only. The I2C caller already owns the bus mutex. */
static void read_sensor(bmp280_t *sensor, unsigned channel, SensorReading *out)
{
  bmp280_status_t result = BMP280_OK;
  if (!sensor->initialized) {
    result = channel == APP_I2C_CHANNEL
        ? bmp280_init_i2c(sensor, &hi2c1, APP_BMP280_ADDRESS)
        : bmp280_init_spi(sensor, &hspi1, GPIOA, GPIO_PIN_4);
  }
  if (result == BMP280_OK) {
    result = bmp280_read_pressure_pa(sensor, &out->pressure_pa);
  }
  out->status = sensor_status(result);
  out->hal_status = sensor->last_status;
  out->hal_error = sensor->last_error;
  app_debug.chip_id[channel] = sensor->chip_id;
  if (result != BMP280_OK) {
    sensor->initialized = false;
    out->pressure_pa = 0;
  }
}

void app_sensor_task(void)
{
  bmp280_t sensors[APP_CHANNELS] = {0};
  SensorReading previous[APP_CHANNELS] = {0};
  uint32_t last_attempt[APP_CHANNELS] = {0};
  bool attempted[APP_CHANNELS] = {false};
  uint32_t sequence = 0;
  uint32_t deadline = now_ms();
  for (;;) {
    SensorRawData_t raw = { .sequence = ++sequence, .timestamp_ms = now_ms() };
    for (unsigned i = 0; i < APP_CHANNELS; ++i) {
      if (!sensors[i].initialized && attempted[i] &&
          (uint32_t)(now_ms() - last_attempt[i]) < APP_RETRY_MS) {
        raw.channel[i] = previous[i]; /* Keep the error, never a stale OK. */
        continue;
      }
      attempted[i] = true;
      if (i == APP_I2C_CHANNEL) {
        if (lock_i2c()) {
          read_sensor(&sensors[i], i, &raw.channel[i]);
          recover_i2c((HAL_StatusTypeDef)raw.channel[i].hal_status,
                       raw.channel[i].hal_error);
          osMutexRelease(i2cmutexHandle);
        } else {
          raw.channel[i].status = PRESSURE_COMM;
          raw.channel[i].hal_status = HAL_BUSY;
          sensors[i].initialized = false;
        }
      } else {
        read_sensor(&sensors[i], i, &raw.channel[i]);
      }
      last_attempt[i] = now_ms();
      previous[i] = raw.channel[i];
    }
    osStatus_t result = osMessageQueuePut(RawQueueHandle, &raw, 0U, 0U);
    if (result == osErrorResource) {
      SensorRawData_t discarded;
      if (osMessageQueueGet(RawQueueHandle, &discarded, NULL, 0U) == osOK) {
        ++app_debug.raw_dropped;
      }
      result = osMessageQueuePut(RawQueueHandle, &raw, 0U, 0U);
    }
    if (result != osOK) { app_panic(APP_FAULT_QUEUE); }
    ++app_debug.sensor_cycles;
    wait_period(&deadline, APP_SENSOR_PERIOD_MS);
  }
}

void app_processing_task(void)
{
  PressureProcessor processor = {0};
  SensorRawData_t raw;
  DisplayData_t result;
  for (;;) {
    if (osMessageQueueGet(RawQueueHandle, &raw, NULL, osWaitForever) != osOK) {
      app_panic(APP_FAULT_QUEUE);
    }
    pressure_process(&processor, &raw, now_ms(), &result);
    osStatus_t status = osMessageQueuePut(DisplayQueueHandle, &result, 0U, 0U);
    if (status == osErrorResource) {
      DisplayData_t discarded;
      if (osMessageQueueGet(DisplayQueueHandle, &discarded, NULL, 0U) == osOK) {
        ++app_debug.display_dropped;
      }
      status = osMessageQueuePut(DisplayQueueHandle, &result, 0U, 0U);
    }
    if (status != osOK) { app_panic(APP_FAULT_QUEUE); }
    ++app_debug.processing_cycles;
  }
}

/* All text output below belongs to DisplayTask, so UART needs no mutex. */
static void uart_line(const char *text)
{
  uint16_t length = 0;
  while (text[length] != '\0') { ++length; }
  if (HAL_UART_Transmit(&huart1, (uint8_t *)text, length,
                        APP_IO_TIMEOUT_MS) != HAL_OK) {
    ++app_debug.uart_failures;
  }
}

static void draw_reading(unsigned channel, const DisplayReading *reading,
                          uint32_t warnings)
{
  char line[22];
  const char *name = channel == APP_I2C_CHANNEL ? "I2C" : "SPI";
  if (reading->status == PRESSURE_OK) {
    snprintf(line, sizeof(line), "%s: %lu.%02lu HPA", name,
             (unsigned long)(reading->pressure_pa / 100U),
             (unsigned long)(reading->pressure_pa % 100U));
  } else {
    const char *value = reading->status == PRESSURE_WAIT ? "WAIT" :
                       reading->status == PRESSURE_STALE ? "STALE" : "ERROR";
    snprintf(line, sizeof(line), "%s: %s", name, value);
  }
  ssd1306_write_line(&oled, channel == 0U ? 2U : 4U, line);
  snprintf(line, sizeof(line), "%s", pressure_status_text(reading->status));
  if (reading->status == PRESSURE_OK) {
    for (unsigned status = PRESSURE_COMM; status <= PRESSURE_STALE; ++status) {
      if ((warnings & PRESSURE_WARNING(channel, status)) != 0U) {
        snprintf(line, sizeof(line), "OK: WAS %s",
                 pressure_status_text((PressureStatus)status));
        break;
      }
    }
  }
  ssd1306_write_line(&oled, channel == 0U ? 3U : 5U, line);
}

static void draw_frame(const DisplayData_t *data)
{
  char line[22];
  ssd1306_write_line(&oled, 0U, "PRESSURE MONITOR");
  draw_reading(0U, &data->channel[0], data->warnings);
  draw_reading(1U, &data->channel[1], data->warnings);
  if (data->channel[0].status == PRESSURE_OK &&
      data->channel[1].status == PRESSURE_OK) {
    snprintf(line, sizeof(line), "DIFF: %lu.%02lu HPA",
             (unsigned long)(data->difference_pa / 100U),
             (unsigned long)(data->difference_pa % 100U));
    ssd1306_write_line(&oled, 6U, line);
  } else {
    ssd1306_write_line(&oled, 6U, "");
  }
  const char *summary = "STATUS: OK";
  if ((data->warnings & PRESSURE_WARNING_MISMATCH) != 0U) {
    summary = "WARN: MISMATCH";
  } else if (data->warnings != 0U) {
    summary = "WARN: SENSOR DATA";
  } else if (data->channel[0].status != PRESSURE_OK ||
             data->channel[1].status != PRESSURE_OK) {
    summary = "STATUS: WAIT";
  }
  ssd1306_write_line(&oled, 7U, summary);
}

static bool update_oled(const DisplayData_t *data)
{
  static uint32_t last_probe;
  uint32_t started = now_ms();
  if (!oled.initialized) {
    if (!lock_i2c()) { return false; }
    bool ready = ssd1306_init(&oled, &hi2c1, APP_OLED_ADDRESS);
    if (!ready) { recover_i2c(oled.last_status, oled.last_i2c_error); }
    osMutexRelease(i2cmutexHandle);
    if (!ready) { return false; }
    last_probe = now_ms();
  } else if ((uint32_t)(now_ms() - last_probe) >= APP_RETRY_MS) {
    /* Dirty rendering may send no data at all. Still detect disconnection. */
    if (!lock_i2c()) { return false; }
    bool ready = ssd1306_probe(&oled);
    if (!ready) { recover_i2c(oled.last_status, oled.last_i2c_error); }
    osMutexRelease(i2cmutexHandle);
    last_probe = now_ms();
    if (!ready) { return false; }
  }
  draw_frame(data); /* Rendering never holds the bus. */
  for (uint8_t page = 0; page < SSD1306_PAGES; ++page) {
    if (oled.dirty_first[page] >= SSD1306_WIDTH) { continue; }
    if (!lock_i2c()) { return false; }
    bool sent = ssd1306_update_page(&oled, page);
    if (!sent) { recover_i2c(oled.last_status, oled.last_i2c_error); }
    osMutexRelease(i2cmutexHandle);
    if (!sent) { return false; }
#if defined(WOKWI_ENABLED)
    /* A software-I2C page is long enough for SensorTask to become ready.
     * The Wokwi port switches only at explicit Thread-mode yield points. */
    osThreadYield();
#endif
  }
  ++app_debug.oled_frames;
  app_debug.oled_data_bytes = oled.data_bytes_sent;
  app_debug.oled_update_ms = now_ms() - started;
  return true;
}

static void sample_resources(void)
{
  app_debug.hal_ms = HAL_GetTick();
  app_debug.rtos_ms = now_ms();
  app_debug.heap_free = (uint32_t)xPortGetFreeHeapSize();
  app_debug.heap_min_free = (uint32_t)xPortGetMinimumEverFreeHeapSize();
  app_debug.stack_free_bytes[0] = osThreadGetStackSpace(SensorTaskHandle);
  app_debug.stack_free_bytes[1] = osThreadGetStackSpace(ProcessingTaskHandle);
  app_debug.stack_free_bytes[2] = osThreadGetStackSpace(DisplayTaskHandle);
}

void app_display_task(void)
{
  char line[192];
  DisplayData_t latest = { .timestamp_ms = now_ms() };
  DisplayData_t logged = {0};
  bool have_log = false, oled_ok = false, oled_attempted = false;
  uint32_t last_oled_attempt = 0, reported_failure = 0;
  uint32_t reported_mutex = 0;
  snprintf(line, sizeof(line),
           "BOOT PRESSURE PORT=%s I2C=%s SPI=%s OLED=DIRTY RTOS=%luHz CPU=%luHz\r\n",
           APP_RTOS_PORT_NAME, APP_I2C_TRANSPORT_NAME, APP_SPI_TRANSPORT_NAME,
           (unsigned long)osKernelGetTickFreq(), (unsigned long)SystemCoreClock);
  uart_line(line);
  if (osKernelGetTickFreq() != 1000U) { app_panic(APP_FAULT_KERNEL); }
  uint32_t start_rtos = now_ms(), start_hal = HAL_GetTick();
  bool clock_reported = false;
  uint32_t stats_time = now_ms(), stats_bytes = 0U;
  uint32_t stats_sensors = 0U;
#if defined(WOKWI_ENABLED)
  uint32_t stats_idle = wokwi_port_idle_calls, stats_switches = wokwi_port_switch_count;
#endif
  osDelay(APP_OLED_BOOT_MS);
  uint32_t deadline = now_ms();
  for (;;) {
    DisplayData_t queued;
    while (osMessageQueueGet(DisplayQueueHandle, &queued, NULL, 0U) == osOK) {
      latest = queued;
    }
    DisplayData_t view = latest;
    pressure_mark_stale(&view, now_ms());
    bool changed = !have_log || view.warnings != logged.warnings;
    for (unsigned i = 0; i < APP_CHANNELS; ++i) {
      changed |= view.channel[i].status != logged.channel[i].status ||
                 view.channel[i].hal_status != logged.channel[i].hal_status ||
                 view.channel[i].hal_error != logged.channel[i].hal_error;
    }
    if (changed) {
      snprintf(line, sizeof(line), "CHIP ID I2C=0x%02lX SPI=0x%02lX expected=0x58\r\n",
               (unsigned long)app_debug.chip_id[0], (unsigned long)app_debug.chip_id[1]);
      uart_line(line);
      for (unsigned i = 0; i < APP_CHANNELS; ++i) {
        const DisplayReading *r = &view.channel[i];
        snprintf(line, sizeof(line), "%s %s filtered_pa=%lu HAL=%lu ERR=0x%08lX\r\n",
                 i == 0U ? "I2C" : "SPI", pressure_status_text(r->status),
                 (unsigned long)r->pressure_pa, (unsigned long)r->hal_status,
                 (unsigned long)r->hal_error);
        uart_line(line);
        for (unsigned status = PRESSURE_COMM; status <= PRESSURE_STALE; ++status) {
          if ((view.warnings & PRESSURE_WARNING(i, status)) != 0U) {
            snprintf(line, sizeof(line), "RECENT %s %s\r\n", i == 0U ? "I2C" : "SPI",
                     pressure_status_text((PressureStatus)status));
            uart_line(line);
          }
        }
      }
      snprintf(line, sizeof(line), "WARN=0x%08lX age_ms=%lu\r\n",
               (unsigned long)view.warnings,
               (unsigned long)(now_ms() - view.timestamp_ms));
      uart_line(line);
      logged = view;
      have_log = true;
    }

    if (oled_ok || !oled_attempted ||
        (uint32_t)(now_ms() - last_oled_attempt) >= APP_RETRY_MS) {
      bool was_ok = oled_ok;
      oled_ok = update_oled(&view);
      if (!oled_ok) { oled.initialized = false; }
      if (!oled_attempted || oled_ok != was_ok) {
        uart_line(oled_ok ? "OLED READY: full frame sent\r\n" : "OLED OFFLINE\r\n");
      }
      oled_attempted = true;
      last_oled_attempt = now_ms();
      if (oled.failure.count != reported_failure) {
        const ssd1306_failure_t *f = &oled.failure;
        snprintf(line, sizeof(line),
                 "OLED FAIL phase=%u cmd=0x%02X page=%u HAL=%u ERR=0x%08lX hal_ms=%lu rtos_ms=%lu count=%lu\r\n",
                 (unsigned)f->phase, (unsigned)f->command, (unsigned)f->page,
                 (unsigned)f->status, (unsigned long)f->i2c_error,
                 (unsigned long)f->hal_tick, (unsigned long)now_ms(),
                 (unsigned long)f->count);
        uart_line(line);
        reported_failure = f->count;
      }
    }
    if (app_debug.mutex_timeouts != reported_mutex) {
      uart_line("I2C MUTEX TIMEOUT\r\n");
      reported_mutex = app_debug.mutex_timeouts;
    }
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13,
        view.warnings != 0U || !oled_ok ? GPIO_PIN_RESET : GPIO_PIN_SET);
    ++app_debug.display_cycles;
    sample_resources();
    if (!clock_reported && (uint32_t)(now_ms() - start_rtos) >= 1000U) {
      snprintf(line, sizeof(line),
               "TIME delta_rtos=%lu delta_hal=%lu heap_free=%lu stacks_free=%lu/%lu/%lu\r\n",
               (unsigned long)(now_ms() - start_rtos),
               (unsigned long)(HAL_GetTick() - start_hal),
               (unsigned long)app_debug.heap_free,
               (unsigned long)app_debug.stack_free_bytes[0],
               (unsigned long)app_debug.stack_free_bytes[1],
               (unsigned long)app_debug.stack_free_bytes[2]);
      uart_line(line);
      clock_reported = true;
    }
    if ((uint32_t)(now_ms() - stats_time) >= APP_DIAGNOSTIC_PERIOD_MS) {
      snprintf(line, sizeof(line),
               "LOAD ms=%lu samples=%lu oled_bytes=%lu last_frame_ms=%lu heap_min=%lu\r\n",
               (unsigned long)(now_ms() - stats_time),
               (unsigned long)(app_debug.sensor_cycles - stats_sensors),
               (unsigned long)(oled.data_bytes_sent - stats_bytes),
               (unsigned long)app_debug.oled_update_ms,
               (unsigned long)app_debug.heap_min_free);
      uart_line(line);
#if defined(WOKWI_ENABLED)
      snprintf(line, sizeof(line), "SCHED idle_calls=%lu switches=%lu\r\n",
               (unsigned long)(wokwi_port_idle_calls - stats_idle),
               (unsigned long)(wokwi_port_switch_count - stats_switches));
      uart_line(line);
      stats_idle = wokwi_port_idle_calls;
      stats_switches = wokwi_port_switch_count;
#endif
      stats_time = now_ms();
      stats_bytes = oled.data_bytes_sent;
      stats_sensors = app_debug.sensor_cycles;
    }
    wait_period(&deadline, APP_DISPLAY_PERIOD_MS);
  }
}
