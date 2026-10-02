/*
 * FreeRTOS Kernel V10.3.1 - STM32F103/Wokwi Thread-mode port.
 * SPDX-License-Identifier: MIT
 *
 * The standard physical-device port remains in ../ARM_CM3.  This simulator
 * port exists because the Wokwi STM32F103 model was observed executing SVC 0
 * without entering SVC_Handler.  It therefore avoids SVC, PendSV and
 * EXC_RETURN while retaining the unmodified FreeRTOS kernel and APIs.
 *
 * The direct context-frame design is adapted from the MIT-licensed
 * Ni-ear/bca182-freertos-multisensor Wokwi compatibility port, commit
 * 66cc9eb48cf1d9a3b6b1cab7d549cef88bee760a (2026).
 */

#include "FreeRTOS.h"
#include "task.h"
#include "stm32f1xx.h"

extern void * volatile pxCurrentTCB;

/* Visible in GDB even when UART/OLED are unavailable. */
volatile uint32_t wokwi_port_tick_count;
volatile uint32_t wokwi_port_switch_count;
volatile BaseType_t wokwi_port_yield_pending;

static UBaseType_t uxCriticalNesting;

static void prvTaskExitError( void ) __attribute__(( used, noinline ));
static void prvTaskBootstrap( void ) __attribute__(( naked ));
static void prvStartFirstTask( void ) __attribute__(( naked ));
static void prvSelectNextTask( void ) __attribute__(( used, noinline ));

/*
 * Software context frame, ten words and eight-byte aligned:
 *   [0..7] r4-r11, [8] resume address, [9] alignment pad.
 * A new task starts through prvTaskBootstrap with r4=entry and r5=argument.
 */
StackType_t *pxPortInitialiseStack( StackType_t *pxTopOfStack,
                                   TaskFunction_t pxCode,
                                   void *pvParameters )
{
    pxTopOfStack -= 10;
    pxTopOfStack[ 0 ] = ( StackType_t ) pxCode;
    pxTopOfStack[ 1 ] = ( StackType_t ) pvParameters;
    pxTopOfStack[ 2 ] = 0x06060606UL;
    pxTopOfStack[ 3 ] = 0x07070707UL;
    pxTopOfStack[ 4 ] = 0x08080808UL;
    pxTopOfStack[ 5 ] = 0x09090909UL;
    pxTopOfStack[ 6 ] = 0x10101010UL;
    pxTopOfStack[ 7 ] = 0x11111111UL;
    pxTopOfStack[ 8 ] = ( ( StackType_t ) prvTaskBootstrap ) | 1UL;
    pxTopOfStack[ 9 ] = 0U;
    return pxTopOfStack;
}

static void prvTaskExitError( void )
{
    taskDISABLE_INTERRUPTS();
    for( ;; ) {
    }
}

static void prvTaskBootstrap( void )
{
    __asm volatile(
        " mov r0, r5                 \n"
        " blx r4                     \n"
        " bl prvTaskExitError        \n"
        " b .                        \n"
    );
}

static void prvSelectNextTask( void )
{
    ++wokwi_port_switch_count;
    vTaskSwitchContext();
}

/* Save the running task and restore the selected task without an exception. */
void vPortYieldDirect( void ) __attribute__(( naked ));
void vPortYieldDirect( void )
{
    __asm volatile(
        " mrs r0, psp                         \n"
        " sub r0, r0, #40                    \n"
        " stmia r0, {r4-r11}                 \n"
        " str lr, [r0, #32]                  \n"
        " movs r1, #0                        \n"
        " str r1, [r0, #36]                  \n"
        " msr psp, r0                        \n"
        " ldr r3, =pxCurrentTCB              \n"
        " ldr r2, [r3]                       \n"
        " str r0, [r2]                       \n"
        " cpsid i                            \n"
        " bl prvSelectNextTask               \n"
        " ldr r3, =pxCurrentTCB              \n"
        " ldr r2, [r3]                       \n"
        " ldr r0, [r2]                       \n"
        " ldmia r0!, {r4-r11}                \n"
        " ldr lr, [r0, #0]                   \n"
        " adds r0, r0, #8                    \n"
        " msr psp, r0                        \n"
        " isb                                \n"
        " cpsie i                            \n"
        " bx lr                              \n"
    );
}

static void prvStartFirstTask( void )
{
    __asm volatile(
        " ldr r3, =pxCurrentTCB              \n"
        " ldr r2, [r3]                       \n"
        " ldr r0, [r2]                       \n"
        " ldmia r0!, {r4-r11}                \n"
        " ldr lr, [r0, #0]                   \n"
        " adds r0, r0, #8                    \n"
        " msr psp, r0                        \n"
        " movs r0, #2                        \n"
        " msr control, r0                    \n"
        " isb                                \n"
        " movs r0, #0                        \n"
        " msr basepri, r0                    \n"
        " cpsie i                            \n"
        " cpsie f                            \n"
        " bx lr                              \n"
    );
}

void vPortRequestYieldFromISR( void )
{
    wokwi_port_yield_pending = pdTRUE;
}

/* CMSIS-RTOS2 owns SysTick_Handler and calls this function.  The ISR advances
 * RTOS time but never changes PSP. */
void xPortSysTickHandler( void )
{
    const uint32_t previousMask = portSET_INTERRUPT_MASK_FROM_ISR();
    ++wokwi_port_tick_count;
    if( xTaskIncrementTick() != pdFALSE ) {
        wokwi_port_yield_pending = pdTRUE;
    }
    portCLEAR_INTERRUPT_MASK_FROM_ISR( previousMask );
}

void vPortSetupTimerInterrupt( void )
{
    SysTick->CTRL = 0U;
    SysTick->VAL = 0U;
    SysTick->LOAD = ( configCPU_CLOCK_HZ / configTICK_RATE_HZ ) - 1UL;
    NVIC_SetPriority( SysTick_IRQn, configLIBRARY_LOWEST_INTERRUPT_PRIORITY );
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk |
                    SysTick_CTRL_ENABLE_Msk;
}

BaseType_t xPortStartScheduler( void )
{
    uxCriticalNesting = 0U;
    wokwi_port_tick_count = 0U;
    wokwi_port_switch_count = 0U;
    wokwi_port_yield_pending = pdFALSE;
    vPortSetupTimerInterrupt();
    prvStartFirstTask();
    return pdFALSE;
}

void vPortEndScheduler( void )
{
    SysTick->CTRL = 0U;
}

void vPortEnterCritical( void )
{
    portDISABLE_INTERRUPTS();
    ++uxCriticalNesting;
}

void vPortExitCritical( void )
{
    configASSERT( uxCriticalNesting > 0U );
    --uxCriticalNesting;
    if( uxCriticalNesting == 0U ) {
        portENABLE_INTERRUPTS();
    }
}

/* Idle is the safe Thread-mode handoff point for a task woken by SysTick. */
void vApplicationIdleHook( void )
{
    __asm volatile( "dsb" ::: "memory" );
    __asm volatile( "wfi" );
    __asm volatile( "isb" );

    if( wokwi_port_yield_pending != pdFALSE ) {
        wokwi_port_yield_pending = pdFALSE;
        vPortYieldDirect();
    }
}

/* Strong vector symbols retained by FreeRTOSConfig; intentionally unused. */
void vPortSVCHandler( void )
{
}

void xPortPendSVHandler( void )
{
}

#if ( configASSERT_DEFINED == 1 )
void vPortValidateInterruptPriority( void )
{
    /* Wokwi reads the STM32F103 NVIC priority probe back as zero. */
}
#endif
