// =============================================================================
//  16.05 -- DMA: the CPU stops carrying bytes
// =============================================================================
//
//  16.03 taught the CPU to carry one byte per interrupt. DMA retires the
//  CPU from the job entirely: a small engine on the bus matrix copies
//  memory to USART2->TDR one byte per TX request, and tells you when the
//  block is done. Interrupt-per-byte costs ~30 cycles of entry/exit per
//  byte; DMA costs a handful of setup writes per BLOCK. At 115200 baud the
//  difference is academic; at 3 Mbaud, or on a SPI display refresh, it is
//  the product.
//
//  A transfer is a contract with THREE signatures on it, and the starter
//  is missing two:
//
//  1. THE ROUTING. DMA1 channel 7 is a generic engine; CSELR's C7S nibble
//     names which peripheral's requests drive it. USART2_TX is request 2
//     on channel 7 (RM0351 11.6.7, table 44). The reset routing is 0 --
//     a peripheral you did not mean. An unrouted channel simply never
//     sees a request: CNDTR sits at its full count forever. No error, no
//     flag. (Newer ST parts replaced CSELR with a DMAMUX -- same idea,
//     more wiring.)
//
//  2. THE CHANNEL. CPAR = &USART2->TDR (never incremented), CMAR = your
//     buffer (MINC: incremented), CNDTR = the count, direction memory ->
//     peripheral, then EN. Program it only while disabled (RM0351 11.4.3).
//     And mind the index: the manual counts channels from 1, the struct
//     array from 0 -- CH[6] is channel 7.
//
//  3. THE PERIPHERAL. The UART must agree to RAISE DMA requests: CR3.DMAT
//     (RM0351 40.8.3). Without it the engine is routed and armed and the
//     UART just... transmits nothing, because nobody asked it to ask.
//
//  When it does not work -- and the starter does not -- debug in that
//  order: is the channel routed (CSELR)? is it armed (CCR.EN, CNDTR
//  loaded)? is the peripheral raising requests (CR3.DMAT)? CNDTR is your
//  witness: stuck at the full count means no requests are arriving.
//
//  TASK
//    Add the two missing signatures to dma_uart_send_start.
//
//  RUN IT
//    ./mec flash 16_05
//
// =============================================================================

#include <mect/mect.h>

#include <stdbool.h>
#include <stdint.h>

#include "bsp.h"
#include "l476_regs.h"

// Not in bsp/l476_regs.h; defined here against the manual.
#define USART_CR3_DMAT (1u << 7)           // RM0351 40.8.3: TX DMA enable
#define DMA_ISR_TCIF7 (1u << 25)           // RM0351 11.6.1: bit 4*(7-1)+1
#define DMA_IFCR_CTCIF7 (1u << 25)         // RM0351 11.6.2: same position
#define DMA_CSELR_C7S_MASK (0xFu << 24)    // RM0351 11.6.7: C7S[3:0]
#define DMA_CSELR_C7S_USART2_TX (2u << 24) // RM0351 table 44: request 2

// --- given: tick ---------------------------------------------------------------

static volatile uint32_t g_ticks;

void SysTick_Handler(void) {
  ++g_ticks;
}

static void systick_init_1khz(void) {
  SYSTICK->RVR = BSP_SYSCLK_HZ / 1000u - 1u;
  SYSTICK->CVR = 0;
  SYSTICK->CSR = SYSTICK_CSR_ENABLE | SYSTICK_CSR_TICKINT | SYSTICK_CSR_CLKSOURCE_CPU;
}

// --- yours ----------------------------------------------------------------------

void dma_uart_send_start(const uint8_t *data, uint32_t len) {
  RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;
  (void)RCC->AHB1ENR;

  // TODO: signature 1 -- route channel 7 to USART2_TX (CSELR).

  // CH[6] IS CHANNEL 7: the manual numbers channels from 1, the array from
  // 0. Every DMA bug report in history checks this line first.
  DMA1->CH[6].CCR = 0; // disable while programming (RM0351 11.4.3)
  DMA1->CH[6].CPAR = (uint32_t)(uintptr_t)&USART2->TDR;
  DMA1->CH[6].CMAR = (uint32_t)(uintptr_t)data;
  DMA1->CH[6].CNDTR = len;

  DMA1->IFCR = DMA_IFCR_CTCIF7; // stale flags, write-1-to-clear (11.04)
  USART2->ICR = USART_ICR_TCCF;

  // TODO: signature 3 -- the UART must agree to raise requests (CR3).

  DMA1->CH[6].CCR = DMA_CCR_MINC | DMA_CCR_DIR_FROM_MEM | DMA_CCR_EN;
}

bool dma_uart_send_done(void) {
  return (DMA1->ISR & DMA_ISR_TCIF7) != 0u;
}

void dma_uart_send_finish(void) {
  DMA1->IFCR = DMA_IFCR_CTCIF7;
  DMA1->CH[6].CCR = 0;
  USART2->CR3 &= ~USART_CR3_DMAT; // hand the UART back to polled printing
}

// --- tests -----------------------------------------------------------------------

static const uint8_t banner[] =
    "\r\n"
    "dma tx: these two hundred bytes travelled from flash to USART2->TDR\r\n"
    "with the CPU doing nothing but counting laps in a while loop. The\r\n"
    "counter below the transfer proves it: DMA moved the data, not code.\r\n";
#define BANNER_LEN ((uint32_t)(sizeof banner - 1u))

static const uint8_t again[] = "dma tx: and once more, re-armed cleanly.\r\n";

TEST("routed, transferred, and the UART agrees (CPU free throughout)") {
  systick_init_1khz();
  dma_uart_send_start(banner, BANNER_LEN);

  // Checks print only on failure, so a passing run stays silent until the
  // wire is quiet -- the harness and the DMA never fight over TDR.
  CHECK_EQ((DMA1->CSELR >> 24) & 0xFu, 2u);

  uint32_t laps = 0;
  const uint32_t start = g_ticks;
  while (!dma_uart_send_done() && (g_ticks - start) < 200u) {
    ++laps; // ~17 ms of wire time; the CPU spends it here, counting
  }
  CHECK(dma_uart_send_done());
  CHECK_EQ(DMA1->CH[6].CNDTR, 0u); // every byte served
  CHECK(laps > 1000u);             // the CPU really was free

  while ((USART2->ISR & USART_ISR_TC) == 0u && (g_ticks - start) < 300u) {
  }
  CHECK_EQ(USART2->ISR & USART_ISR_TC, USART_ISR_TC);
  dma_uart_send_finish();
}

TEST("a second transfer re-arms cleanly") {
  const uint32_t start = g_ticks;
  dma_uart_send_start(again, (uint32_t)(sizeof again - 1u));
  while (!dma_uart_send_done() && (g_ticks - start) < 100u) {
  }
  CHECK(dma_uart_send_done());
  CHECK_EQ(DMA1->CH[6].CNDTR, 0u);
  dma_uart_send_finish();
}
