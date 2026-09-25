// Solution -- 15.05 UART transmit: the two flags everyone confuses

#include <mect/mect.h>

#include <stdint.h>

#include "bsp.h"
#include "l476_regs.h"

uint32_t brr_for_baud(uint32_t clk_hz, uint32_t baud) {
  // Add half the divisor before dividing: the integer spelling of "round
  // to nearest". 00.01 taught the multiply-first half of integer hygiene;
  // this is the rounding half.
  return (clk_hz + baud / 2u) / baud;
}

void uart2_send_line(const char *s) {
  for (; *s != '\0'; ++s) {
    while ((USART2->ISR & USART_ISR_TXE) == 0u) {
    }
    USART2->TDR = (uint8_t)*s;
  }
  while ((USART2->ISR & USART_ISR_TXE) == 0u) {
  }
  USART2->TDR = '\r';
  while ((USART2->ISR & USART_ISR_TXE) == 0u) {
  }
  USART2->TDR = '\n';
  // The contract said "on the wire", and only TC can promise that: it sets
  // when the shift register runs dry after the last mailbox byte. This is
  // the wait you need before STOP mode, before disabling TE, before
  // flipping an RS-485 direction pin -- before any change the last byte
  // must beat.
  while ((USART2->ISR & USART_ISR_TC) == 0u) {
  }
}

TEST("BRR rounds to the nearest divider") {
  CHECK_EQ(brr_for_baud(4000000, 115200), 35u);   // 34.72 -> 35, not 34
  CHECK_EQ(brr_for_baud(4000000, 9600), 417u);    // 416.67 -> 417
  CHECK_EQ(brr_for_baud(80000000, 115200), 694u); // 694.44 -> 694: they agree
  // ... and the BSP agrees with you:
  CHECK_EQ(USART2->BRR, brr_for_baud(BSP_SYSCLK_HZ, 115200));
}

TEST("send_line does not return until the wire is idle") {
  USART2->ICR = USART_ICR_TCCF; // arm: clear Transmission Complete (w1c)
  uart2_send_line("TC before TXE convenience, always");
  CHECK_EQ(USART2->ISR & USART_ISR_TC, USART_ISR_TC);
}

TEST("say something a human can see") {
  uart2_send_line("uart2_send_line: hello from PA2, via the ST-Link bridge");
  CHECK(1); // the proof is the line above this test's [ ok ] marker
}
