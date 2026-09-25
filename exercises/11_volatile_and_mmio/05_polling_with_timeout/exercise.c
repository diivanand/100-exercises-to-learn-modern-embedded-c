// =============================================================================
//  11.05 -- Poll with a deadline, and look once more before giving up
// =============================================================================
//
//  Hardware fails. An external chip browns out, a crystal never starts, a
//  cable is unplugged, and the flag you are polling for stays low forever.
//  Code that waits like this
//
//      while ((USART2->ISR & TXE) == 0) { }
//
//  turns any of those into a firmware hang -- often with the watchdog reset
//  (17.04) as the only diagnosis. The rule: EVERY wait on hardware carries
//  a deadline, and a missed deadline is an ERROR the caller hears about
//  (chapter 09), not a reason to keep waiting.
//
//  Two design questions, answered here:
//
//   - HOW LONG? From the datasheet's worst case, times a margin. A byte at
//     115200 baud takes ~87 us to shift out; a 2 ms budget for TXE is three
//     orders of magnitude of slack -- generous enough to never fire
//     spuriously, tight enough that a dead UART is caught within one
//     control-loop tick.
//
//   - AND THEN WHAT? Return WAIT_TIMEOUT. Do not retry inside the wait
//     (that is just a longer wait), and do not decide policy down here --
//     whether to reset the peripheral, log, or reboot belongs to a caller
//     that can see more than one register.
//
//  There is a boundary subtlety worth getting right, and the third test
//  probes it. Your loop looks at the FLAG, then at the CLOCK. Time passes
//  between the two. If the deadline check is the one that exits the loop,
//  the flag may have risen since you last looked -- so look ONCE MORE
//  before declaring failure. Without that final look, a flag that arrives
//  exactly on the boundary becomes a spurious timeout: the peripheral did
//  its job on time and got blamed anyway. (Rare, load-dependent, and
//  infuriating in the field. The pattern costs one line.)
//
//  The clock is an injected function (10.04's fake-clock seam): the tests
//  script time itself, so "exactly at the deadline" is a reproducible
//  case, not a lucky race.
//
//  TASK
//    Rewrite `uart_wait_flag` to honour `timeout_ms` using `now_ms`,
//    including the final look. Do not change the scripted hardware or the
//    tests.
//
//  RUN IT
//    ./mec test 11_05
//
// =============================================================================

#include <mect/mect.h>

#include <stdint.h>

#define UART_ISR_TXE (1u << 7)

enum wait_status { WAIT_OK = 0, WAIT_TIMEOUT = -1 };

typedef uint32_t (*tick_fn)(void);

int uart_wait_flag(const volatile uint32_t *isr, uint32_t mask, tick_fn now_ms,
                   uint32_t timeout_ms) {
  // TODO: this waits forever, and it never even consults the clock.
  (void)now_ms;
  (void)timeout_ms;
  while ((*isr & mask) == 0u) {
  }
  return WAIT_OK;
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
  CHECK_EQ(uart_wait_flag(&fake_isr, UART_ISR_TXE, scripted_tick, 10),
           WAIT_TIMEOUT);
}

TEST("a flag that arrives exactly at the deadline still succeeds") {
  script(5); // raised during the clock read that exceeds the timeout
  CHECK_EQ(uart_wait_flag(&fake_isr, UART_ISR_TXE, scripted_tick, 4), WAIT_OK);
}
