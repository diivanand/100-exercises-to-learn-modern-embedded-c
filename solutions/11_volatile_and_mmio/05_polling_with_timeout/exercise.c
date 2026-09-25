// Solution -- 11.05 Poll with a deadline, and look once more before giving up

#include <mect/mect.h>

#include <stdint.h>

#define UART_ISR_TXE (1u << 7)

enum wait_status { WAIT_OK = 0, WAIT_TIMEOUT = -1 };

typedef uint32_t (*tick_fn)(void);

int uart_wait_flag(const volatile uint32_t *isr, uint32_t mask, tick_fn now_ms,
                   uint32_t timeout_ms) {
  const uint32_t start = now_ms();
  do {
    if ((*isr & mask) != 0u) {
      return WAIT_OK;
    }
    // Unsigned subtraction handles the tick counter wrapping (13.04 makes
    // a whole exercise of this); <= keeps the full timeout window open.
  } while (now_ms() - start <= timeout_ms);

  // The deadline has passed -- but time also passed between our last look
  // at the flag and our look at the clock. One more look, so a flag that
  // arrived on the boundary is a success, not a spurious timeout.
  if ((*isr & mask) != 0u) {
    return WAIT_OK;
  }
  return WAIT_TIMEOUT;
}

// --- scripted hardware ---------------------------------------------------------
// The fake clock advances one millisecond per call, and raises the flag on
// schedule -- the same controlled-time trick as 10.04, pointed at MMIO.

static volatile uint32_t fake_isr;
static uint32_t fake_now;
static uint32_t flag_arrives_at; // 0 = never

static uint32_t scripted_tick(void) {
  if (flag_arrives_at != 0u && fake_now >= flag_arrives_at) {
    fake_isr |= UART_ISR_TXE;
  }
  return fake_now++;
}

static void script(uint32_t arrives_at) {
  fake_isr = 0;
  fake_now = 0;
  flag_arrives_at = arrives_at;
}

TEST("a flag that is already set succeeds, even with timeout zero") {
  script(0);
  fake_isr = UART_ISR_TXE;
  CHECK_EQ(uart_wait_flag(&fake_isr, UART_ISR_TXE, scripted_tick, 0), WAIT_OK);
}

TEST("a flag that never comes is a timeout, not a hang") {
  script(0); // never arrives
  CHECK_EQ(uart_wait_flag(&fake_isr, UART_ISR_TXE, scripted_tick, 10), WAIT_TIMEOUT);
}

TEST("a flag that arrives exactly at the deadline still succeeds") {
  script(5); // raised during the clock read that exceeds the timeout
  CHECK_EQ(uart_wait_flag(&fake_isr, UART_ISR_TXE, scripted_tick, 4), WAIT_OK);
}
