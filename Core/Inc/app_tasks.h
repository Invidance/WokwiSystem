#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdint.h>

typedef enum {
  APP_FAULT_NONE = 0,
  APP_FAULT_KERNEL,
  APP_FAULT_OBJECT,
  APP_FAULT_QUEUE,
  APP_FAULT_HEAP,
  APP_FAULT_STACK,
  APP_FAULT_HAL
} AppFault;

/* Read-only diagnostics for GDB. No application decisions depend on these. */
typedef struct {
  uint32_t sensor_cycles, processing_cycles, display_cycles, oled_frames;
  uint32_t raw_dropped, display_dropped, mutex_timeouts, i2c_recoveries;
  uint32_t recovery_status, uart_failures;
  uint32_t hal_ms, rtos_ms, heap_free, heap_min_free;
  uint32_t stack_free_bytes[3];
} AppDebug;

extern volatile AppDebug app_debug;
extern volatile AppFault app_fault;
extern const char * volatile app_fault_task;

void app_sensor_task(void);
void app_processing_task(void);
void app_display_task(void);
_Noreturn void app_panic(AppFault fault);

#endif
