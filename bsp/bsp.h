// Board support for the NUCLEO-L476RG: the minimum a test harness needs.
//
// Deliberately small. The board boots on its 4 MHz MSI clock (no PLL, no
// flash wait-state dance) and brings up USART2 at 115200 so mect can print.
// Everything else -- LEDs, buttons, timers, interrupts, DMA -- is the
// exercises' job, register by register.

#ifndef BSP_BSP_H
#define BSP_BSP_H

#include <stdbool.h>
#include <stdint.h>

// The clock everything runs from after reset: the Multi-Speed Internal
// oscillator at its default 4 MHz (RM0351 6.2.4). Exercises that configure
// SysTick or a UART derive their arithmetic from this.
#define BSP_SYSCLK_HZ 4000000u

// Called by Reset_Handler before main(): GPIOA clock, PA2/PA3 to AF7,
// USART2 at 115200 8N1.
void bsp_init(void);

// Blocking transmit of one byte over the ST-Link virtual COM port.
void bsp_uart_putc(char c);

// Non-blocking receive: true and *out filled if a byte was waiting.
bool bsp_uart_try_getc(uint8_t *out);

#endif // BSP_BSP_H
