#include "wokwi_spi.h"

static void settle(void)
{
  /* Explicit setup/hold time, including when compiled with optimisation. */
  __asm volatile("nop\n nop\n nop\n nop\n nop\n nop\n nop\n nop" ::: "memory");
}

void wokwi_spi_init(void)
{
  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIOA->BRR = GPIO_PIN_5 | GPIO_PIN_7;
  GPIO_InitTypeDef pins = {0};
  pins.Pin = GPIO_PIN_5 | GPIO_PIN_7;
  pins.Mode = GPIO_MODE_OUTPUT_PP;
  pins.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &pins);
  pins.Pin = GPIO_PIN_6;
  pins.Mode = GPIO_MODE_INPUT;
  pins.Pull = GPIO_PULLUP; /* An absent slave reads FF, not a plausible ID. */
  HAL_GPIO_Init(GPIOA, &pins);
  settle();
}

uint8_t wokwi_spi_exchange(uint8_t value)
{
  uint8_t received = 0U;
  for (unsigned bit = 0; bit < 8U; ++bit) {
    if ((value & 0x80U) != 0U) { GPIOA->BSRR = GPIO_PIN_7; }
    else { GPIOA->BRR = GPIO_PIN_7; }
    settle();
    GPIOA->BSRR = GPIO_PIN_5;
    settle();
    received = (uint8_t)((received << 1U) |
                        ((GPIOA->IDR & GPIO_PIN_6) != 0U));
    GPIOA->BRR = GPIO_PIN_5;
    settle();
    value <<= 1U;
  }
  return received;
}
