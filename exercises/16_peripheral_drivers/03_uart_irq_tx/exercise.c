// =============================================================================
//  16.03 -- Interrupt-driven transmit: the TXEIE storm
// =============================================================================
//
//  15.05's send loop burnt 87 microseconds of CPU per byte, waiting. The
//  grown-up transmitter is 12.07's deferred-work shape run in reverse:
//  mainline ENQUEUES bytes into a 12.04 SPSC ring and walks away; the
//  USART2 interrupt drains the ring one byte per TXE, on the wire's own
//  schedule. Main pays nanoseconds per byte; the UART paces itself.
//
//  The one idea that makes it work -- and the one bug that makes it a
//  brick:
//
//  TXE IS A LEVEL, NOT AN EVENT. "Transmit register empty" stays true for
//  as long as it is true. While TXEIE is set, an empty TDR means the
//  interrupt fires THE MOMENT the handler returns. That is exactly what
//  you want while the ring has bytes: send one, return, re-enter for the
//  next. But when the ring runs dry and you return WITHOUT feeding TDR and
//  WITHOUT clearing TXEIE, the level is still raised -- the handler
//  re-enters instantly, finds nothing, returns, re-enters... Mainline
//  never runs another instruction. The rule: AN INTERRUPT SOURCE YOU
//  CANNOT FEED MUST BE MUTED before the handler returns; enqueueing
//  re-arms it.
//
//  The starter has exactly that bug, plus a lazier one in the free-space
//  arithmetic. THE STARTER'S SYMPTOM ON THE BOARD IS A HANG: the first
//  test's line drains, the storm begins, and `./mec flash` reports the
//  deadline. That silence IS the diagnostic -- remember what it looks
//  like.
//
//  COEXISTENCE NOTE. The harness prints through the BLOCKING putc from
//  15.05 on the same UART. The two transmitters never fight because every
//  test drains the ring completely (and TXEIE is then off) before its
//  checks print. In a real firmware you route everything through one
//  path; here the contrast is the lesson.
//
//  TASK
//    Fix uart2_write_async's free-space accounting (refuse and report what
//    does not fit -- 12.07's drop policy) and the ISR's empty case.
//
//  RUN IT
//    ./mec flash 16_03
//
// =============================================================================

#include <mect/mect.h>

#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>

#include "bsp.h"
#include "l476_regs.h"

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

// --- given: the 12.04 SPSC ring, condensed --------------------------------------
// Producer = main (head), consumer = the ISR (tail). Free-running indices,
// masked on access; release on publish, acquire on read. See 12.04.

enum { RING_CAPACITY = 128 }; // power of two, so & (CAP - 1) wraps

static uint8_t ring_slots[RING_CAPACITY];
static atomic_uint ring_head; // next slot main will fill
static atomic_uint ring_tail; // next slot the ISR will send

static volatile uint32_t g_sent; // bytes the ISR has pushed into TDR

// --- yours ----------------------------------------------------------------------

void uart_irq_init(void) {
  nvic_enable_irq(IRQN_USART2);
}

size_t uart2_write_async(const uint8_t *data, size_t n) {
  const unsigned head = atomic_load_explicit(&ring_head, memory_order_relaxed);

  // TODO: "128 bytes is plenty" is not an accounting policy. How much room
  // is really left? (head, tail, and 13.04's subtraction have the answer.)
  const unsigned free_slots = (unsigned)RING_CAPACITY;
  const size_t accepted = (n < free_slots) ? n : free_slots;

  for (size_t i = 0; i < accepted; ++i) {
    ring_slots[(head + (unsigned)i) & (RING_CAPACITY - 1u)] = data[i];
  }
  atomic_store_explicit(&ring_head, head + (unsigned)accepted, memory_order_release);
  USART2->CR1 |= USART_CR1_TXEIE;
  return accepted;
}

void USART2_IRQHandler(void) {
  if ((USART2->ISR & USART_ISR_TXE) != 0u) {
    const unsigned tail = atomic_load_explicit(&ring_tail, memory_order_relaxed);
    const unsigned head = atomic_load_explicit(&ring_head, memory_order_acquire);
    if (head == tail) {
      // TODO: the level is still raised. See the header before the board
      // teaches you the hard way.
      return;
    }
    USART2->TDR = ring_slots[tail & (RING_CAPACITY - 1u)];
    atomic_store_explicit(&ring_tail, tail + 1u, memory_order_release);
    ++g_sent;
  }
}

// --- tests -----------------------------------------------------------------------

// sizeof - 1: send the text, not its terminating NUL.
static const uint8_t line_a[] = "async tx [A]: bytes on interrupts, "
                                "main loop still alive...\r\n";
static const uint8_t line_b[] = "async tx [B]: mid-flight ok\r\n";
#define LEN_A (sizeof line_a - 1u)
#define LEN_B (sizeof line_b - 1u)

TEST("a line leaves while main keeps running") {
  systick_init_1khz();
  uart_irq_init();
  g_sent = 0;

  CHECK_EQ(uart2_write_async(line_a, LEN_A), LEN_A);

  uint32_t progress = 0;
  const uint32_t start = g_ticks;
  while (g_sent < LEN_A && (g_ticks - start) < 100u) {
    ++progress; // main is FREE during the send; that is the whole point
  }
  CHECK_EQ(g_sent, (uint32_t)LEN_A);
  CHECK(progress > 100u); // a blocking send would leave this near zero

  while ((USART2->ISR & USART_ISR_TC) == 0u && (g_ticks - start) < 200u) {
  }
  CHECK_EQ(USART2->ISR & USART_ISR_TC, USART_ISR_TC);
}

TEST("queueing during an in-flight send works") {
  g_sent = 0;
  CHECK_EQ(uart2_write_async(line_a, LEN_A), LEN_A);
  CHECK_EQ(uart2_write_async(line_b, LEN_B), LEN_B);

  const uint32_t start = g_ticks;
  const uint32_t expect = (uint32_t)(LEN_A + LEN_B);
  while (g_sent < expect && (g_ticks - start) < 200u) {
  }
  CHECK_EQ(g_sent, expect);
}

TEST("overflow is refused and reported, not absorbed") {
  uint8_t big[200];
  for (size_t i = 0; i < sizeof big; ++i) {
    big[i] = (uint8_t)('0' + (i % 10u));
  }
  g_sent = 0;
  const size_t accepted = uart2_write_async(big, sizeof big);
  // The ring holds 128; a byte or two may drain during the call itself.
  CHECK(accepted <= RING_CAPACITY + 2u);
  CHECK(accepted >= 100u);

  const uint32_t start = g_ticks;
  while (g_sent < accepted && (g_ticks - start) < 200u) {
  }
  CHECK_EQ(g_sent, (uint32_t)accepted);
  while ((USART2->ISR & USART_ISR_TC) == 0u && (g_ticks - start) < 300u) {
  }
  CHECK_EQ(USART2->ISR & USART_ISR_TC, USART_ISR_TC);
}
