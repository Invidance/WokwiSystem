/*
 * FreeRTOS Kernel V10.3.1 - Wokwi STM32F103 compatibility overrides.
 * SPDX-License-Identifier: MIT
 */

#ifndef PORTMACRO_WOKWI_H
#define PORTMACRO_WOKWI_H

/* Reuse all Cortex-M3 types, interrupt masks and alignment definitions. */
#include "../ARM_CM3/portmacro.h"

/* The Wokwi port switches only from Thread mode.  An ISR merely records that
 * a yield is needed; the idle hook or the next blocking API performs it. */
extern void vPortYieldDirect( void );
extern void vPortRequestYieldFromISR( void );

/* The simulator does not retain implemented NVIC priority bits, so BASEPRI
 * cannot reliably exclude TIM2/SysTick from kernel critical sections.  The
 * Wokwi-only port uses PRIMASK instead. */
portFORCE_INLINE static uint32_t ulPortWokwiSetInterruptMask( void )
{
    uint32_t previousMask;
    __asm volatile(
        " mrs %0, primask \n"
        " cpsid i         \n"
        " dsb             \n"
        " isb             \n"
        : "=r" ( previousMask ) :: "memory"
    );
    return previousMask;
}

portFORCE_INLINE static void vPortWokwiRestoreInterruptMask( uint32_t mask )
{
    __asm volatile(
        " msr primask, %0 \n"
        " dsb             \n"
        " isb             \n"
        :: "r" ( mask ) : "memory"
    );
}

portFORCE_INLINE static void vPortWokwiDisableInterrupts( void )
{
    __asm volatile( "cpsid i" ::: "memory" );
}

portFORCE_INLINE static void vPortWokwiEnableInterrupts( void )
{
    __asm volatile( "cpsie i" ::: "memory" );
}

#undef portYIELD
#undef portEND_SWITCHING_ISR
#undef portYIELD_FROM_ISR
#undef portSET_INTERRUPT_MASK_FROM_ISR
#undef portCLEAR_INTERRUPT_MASK_FROM_ISR
#undef portDISABLE_INTERRUPTS
#undef portENABLE_INTERRUPTS

#define portYIELD() vPortYieldDirect()
#define portEND_SWITCHING_ISR( xSwitchRequired )                         \
    do {                                                                 \
        if( ( xSwitchRequired ) != pdFALSE ) {                            \
            vPortRequestYieldFromISR();                                   \
        }                                                                 \
    } while( 0 )
#define portYIELD_FROM_ISR( xSwitchRequired ) \
    portEND_SWITCHING_ISR( xSwitchRequired )

#define portSET_INTERRUPT_MASK_FROM_ISR() ulPortWokwiSetInterruptMask()
#define portCLEAR_INTERRUPT_MASK_FROM_ISR( mask ) \
    vPortWokwiRestoreInterruptMask( mask )
#define portDISABLE_INTERRUPTS() vPortWokwiDisableInterrupts()
#define portENABLE_INTERRUPTS() vPortWokwiEnableInterrupts()

#endif /* PORTMACRO_WOKWI_H */
