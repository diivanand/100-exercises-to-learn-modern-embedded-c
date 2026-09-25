// Board support implementation. Chapter 15 has you rebuild most of this
// yourself; it is written the way the exercises teach it (11.02-11.06).

#include "bsp.h"

#include "l476_regs.h"

void bsp_init(void) {
  // Clock the peripherals we use. Read the register back once afterwards:
  // the write must reach the RCC before the peripheral behind it wakes
  // (RM0351 6.2.17 note on peripheral clock enable delay).
  RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
  RCC->APB1ENR1 |= RCC_APB1ENR1_USART2EN;
  (void)RCC->APB1ENR1;

  // PA2 (TX) and PA3 (RX) to alternate function 7 = USART2 (STM32L476
  // datasheet, table 16). MODER: 2 bits per pin, 10 = alternate function.
  // Clear-then-set, never plain OR -- 11.03 is about exactly this.
  GPIOA->MODER &= ~((3u << (2u * 2u)) | (3u << (3u * 2u)));
  GPIOA->MODER |= (2u << (2u * 2u)) | (2u << (3u * 2u));
  GPIOA->AFR[0] &= ~((0xFu << (2u * 4u)) | (0xFu << (3u * 4u)));
  GPIOA->AFR[0] |= (7u << (2u * 4u)) | (7u << (3u * 4u));

  // 115200 baud from a 4 MHz kernel clock, oversampling by 16:
  // BRR = 4000000 / 115200 = 34.7 -> 35, giving 114286 baud (-0.8%, within
  // the 2.5% a UART tolerates).
  USART2->BRR = (BSP_SYSCLK_HZ + 115200u / 2u) / 115200u;
  USART2->CR1 = USART_CR1_TE | USART_CR1_RE | USART_CR1_UE;
}

void bsp_uart_putc(char c) {
  while ((USART2->ISR & USART_ISR_TXE) == 0u) {
    // TXE: transmit data register empty. Polling with no timeout is honest
    // here -- if the UART never drains, there is nothing to report an
    // error THROUGH. Exercise 11.05 does it properly for everything else.
  }
  USART2->TDR = (uint8_t)c;
}

bool bsp_uart_try_getc(uint8_t *out) {
  if ((USART2->ISR & USART_ISR_RXNE) == 0u) {
    return false;
  }
  *out = (uint8_t)USART2->RDR; // reading RDR clears RXNE (11.04)
  return true;
}
