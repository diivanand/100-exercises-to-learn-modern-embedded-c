// =============================================================================
//  15.05 -- UART transmit: the two flags everyone confuses
// =============================================================================
//
//  Every character this course has printed at you travelled through USART2,
//  configured by bsp/bsp.c. Time to own that code. Two lessons:
//
//  BAUD IS ARITHMETIC. With oversampling by 16, BRR is simply the kernel
//  clock divided by the baud rate (RM0351 40.5.4) -- but ROUNDED, not
//  truncated. At 4 MHz and 115200 the true divider is 34.72: truncating to
//  34 puts you 2.1% fast, rounding to 35 is 0.8% slow. A UART tolerates
//  about 2.5% of accumulated mismatch before it samples a bit wrongly, so
//  the lazy division spends most of the budget before the cable gets a say.
//  (This is 00.01's integer-division lesson with a protocol attached:
//  add half the divisor before dividing.)
//
//  TXE IS NOT TC. The transmitter is a two-stage pipeline: TDR (the
//  mailbox) feeds a shift register (the wire).
//
//    TXE  "the mailbox is empty" -- you may write the NEXT byte. Right for
//         pacing a loop; says nothing about the wire.
//    TC   "mailbox empty AND shift register idle" -- the LAST byte has
//         fully left the pin.
//
//  When does the difference matter? Whenever the world changes after your
//  "send" returns: entering STOP mode (the UART clock dies mid-byte),
//  disabling the transmitter, switching the direction pin of an RS-485
//  transceiver. Wait on TXE per byte, and on TC once, at the end.
//
//  TC is cleared by writing TCCF into ICR -- the write-1-to-clear register
//  you met in 11.04. The tests use it to arm the trap.
//
//  TASK
//    Fix brr_for_baud (rounding) and uart2_send_line (the missing TC wait).
//
//  RUN IT
//    ./mec flash 15_05
//
// =============================================================================

#include <mect/mect.h>

#include "bsp.h"
#include "l476_regs.h"

#include <stdint.h>

// Divider for oversampling-by-16, ROUNDED to the nearest integer.
uint32_t brr_for_baud(uint32_t clk_hz, uint32_t baud) {
  // TODO: this truncates.
  return clk_hz / baud;
}

// Send a string followed by CRLF; when this returns, the line must be ON
// THE WIRE in its entirety, not merely queued.
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
  // TODO: TXE said the mailbox is empty. Nobody said the wire is.
}

TEST("BRR rounds to the nearest divider") {
  CHECK_EQ(brr_for_baud(4000000, 115200), 35u);  // 34.72 -> 35, not 34
  CHECK_EQ(brr_for_baud(4000000, 9600), 417u);   // 416.67 -> 417
  CHECK_EQ(brr_for_baud(80000000, 115200), 694u); // 694.44 -> 694: they agree
  // ... and the BSP agrees with you:
  CHECK_EQ(USART2->BRR, brr_for_baud(BSP_SYSCLK_HZ, 115200));
}

TEST("send_line does not return until the wire is idle") {
  USART2->ICR = USART_ICR_TCCF; // arm: clear Transmission Complete (w1c)
  uart2_send_line("TC before TXE convenience, always");
  // If send_line only waited for TXE, the last byte is still shifting out
  // right now and TC is 0. The check runs nanoseconds after the return;
  // a byte takes 87 microseconds. There is no race to win here.
  CHECK_EQ(USART2->ISR & USART_ISR_TC, USART_ISR_TC);
}

TEST("say something a human can see") {
  uart2_send_line("uart2_send_line: hello from PA2, via the ST-Link bridge");
  CHECK(1); // the proof is the line above this test's [ ok ] marker
}
