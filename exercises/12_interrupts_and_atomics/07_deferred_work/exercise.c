// =============================================================================
//  12.07 -- Deferred work: the ISR posts, the main loop does
// =============================================================================
//
//  Everything in this chapter assembles into one architectural rule, and it
//  is the shape of essentially all sound firmware:
//
//      AN ISR MOVES DATA AND SETS FLAGS. NOTHING ELSE.
//
//  Why: while your handler runs, every same-or-lower-priority interrupt in
//  the system WAITS. Parse a protocol in the UART handler and the motor
//  control loop jitters; call printf there and you have bought milliseconds
//  of latency at 8 bits per 87 microseconds. The handler's job is to get
//  the byte somewhere safe (the 12.04 ring), note that something happened
//  (the 12.02 event bits), and return -- the main loop does the real work
//  at leisure, in main context, where it can be as slow as it likes.
//  (An RTOS's "bottom half" / deferred procedure / task notification is
//  this same rule with an OS's vocabulary; 16.01 does it on real hardware
//  with the blue button's EXTI interrupt.)
//
//  Overload needs a POLICY, decided at design time. This module's contract:
//  when the ring is full, the ISR drops the byte and increments a counter.
//  Dropping under overload is a legitimate policy; losing COUNT of what was
//  dropped is a bug. An ISR that blocks -- waiting for the main loop to
//  make room -- is not a policy, it is a deadlock with a UART attached.
//
//  THE INSTRUMENTATION. parse_one_byte() bills each unit of parsing work to
//  the context that ran it, by thread identity (the given block below).
//  The tests then assert the invariant this whole exercise exists for:
//  rx_work_done_in_isr() == 0. The starter parses inline in the handler --
//  functionally "correct", every line arrives -- and fails on exactly that
//  invariant, plus the missing drop accounting. Deadlines, not just
//  results, are what real-time means.
//
//  TASK
//    Restructure: uart_rx_isr() only pushes into an SPSC ring (12.04's
//    release/acquire discipline; count drops when full), rx_poll() drains
//    the ring and calls parse_one_byte() from main context, and rx_init()
//    resets the lot. Do not change the instrumentation or the tests.
//
//  RUN IT
//    ./mec test 12_07
//
// =============================================================================

#include <mect/mect.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>

// --- instrumentation (given; do not edit) --------------------------------------

static pthread_t g_main_thread;
static _Atomic uint32_t g_work_in_isr;   // parse work billed to the "ISR"
static _Atomic uint32_t g_lines;         // completed lines
static _Atomic uint32_t g_line_checksum; // XOR-fold of every finished line

static void rx_instrument_reset(void) {
  g_main_thread = pthread_self();
  atomic_store(&g_work_in_isr, 0u);
  atomic_store(&g_lines, 0u);
  atomic_store(&g_line_checksum, 0u);
}

// Parse one byte of a newline-terminated line: the WORK this module exists
// to do. Costs one work unit, billed to whichever context runs it.
static void parse_one_byte(uint8_t byte) {
  static uint8_t line_xor;
  if (!pthread_equal(pthread_self(), g_main_thread)) {
    atomic_fetch_add(&g_work_in_isr, 1u);
  }
  if (byte == (uint8_t)'\n') {
    atomic_fetch_add(&g_lines, 1u);
    atomic_fetch_xor(&g_line_checksum, line_xor);
    line_xor = 0;
  } else {
    line_xor = (uint8_t)(line_xor ^ byte);
  }
}

// --- the code under test ------------------------------------------------------

enum { RX_RING_CAPACITY = 128 };

void rx_init(void) {
  rx_instrument_reset();
  // TODO: reset your ring and drop counter here too.
}

void uart_rx_isr(uint8_t byte) {
  // TODO: this does the WORK in interrupt context, and when the system is
  // too busy it silently loses data with no accounting. Push into a ring
  // (12.04), count a drop when it is full, and leave.
  parse_one_byte(byte);
}

void rx_poll(void) {
  // TODO: drain the ring here, in main context, and parse.
}

uint32_t rx_lines(void) {
  return atomic_load(&g_lines);
}

uint32_t rx_dropped(void) {
  // TODO: report the drop counter.
  return 0u;
}

uint32_t rx_work_done_in_isr(void) {
  return atomic_load(&g_work_in_isr);
}

uint32_t rx_line_checksum(void) {
  return atomic_load(&g_line_checksum);
}

// --- the tests ------------------------------------------------------------------

// The traffic generator: line k is "Tkkkk\n" with k in decimal-ish nibbles;
// what matters is that the test can compute the expected XOR fold itself.
static uint8_t expected_fold(uint32_t line_count) {
  uint8_t fold = 0;
  for (uint32_t k = 0; k < line_count; ++k) {
    uint8_t line_xor = (uint8_t)'T';
    for (int d = 3; d >= 0; --d) {
      const uint8_t digit = (uint8_t)('0' + ((k >> (4u * (uint32_t)d)) & 0xFu));
      line_xor = (uint8_t)(line_xor ^ digit);
    }
    fold = (uint8_t)(fold ^ line_xor);
  }
  return fold;
}

static void send_line(void (*put)(uint8_t), uint32_t k) {
  put((uint8_t)'T');
  for (int d = 3; d >= 0; --d) {
    put((uint8_t)('0' + ((k >> (4u * (uint32_t)d)) & 0xFu)));
  }
  put((uint8_t)'\n');
}

TEST("a quiet stream is parsed completely, with no work in the ISR") {
  rx_init();
  for (uint32_t k = 0; k < 40; ++k) {
    send_line(uart_rx_isr, k);
    rx_poll();
  }
  CHECK_EQ(rx_lines(), 40u);
  CHECK_EQ(rx_dropped(), 0u);
  CHECK_EQ(rx_line_checksum(), (uint32_t)expected_fold(40));
  CHECK_EQ(rx_work_done_in_isr(), 0u); // single-threaded: trivially true
}

TEST("a burst with nobody polling drops bytes -- and COUNTS them") {
  rx_init();
  for (uint32_t i = 0; i < 500; ++i) {
    uart_rx_isr((uint8_t)'A'); // no newline: nothing completes
  }
  // 128 buffered, the rest dropped and accounted for. Losing data under
  // overload is a policy; losing COUNT of it is a bug.
  CHECK_EQ(rx_dropped(), 500u - (uint32_t)RX_RING_CAPACITY);
  rx_poll();
  CHECK_EQ(rx_lines(), 0u);
}

enum { SOAK_LINES = 2000 };

static _Atomic uint32_t g_sent_lines;

static void *isr_feeder(void *arg) {
  (void)arg;
  for (uint32_t k = 0; k < (uint32_t)SOAK_LINES; ++k) {
    send_line(uart_rx_isr, k);
    atomic_store(&g_sent_lines, k + 1u);
    // Flow control, as a real baud rate provides: at most ~4 lines
    // (24 bytes) in flight, so a polling consumer never overruns the ring.
    uint64_t patience = 0;
    while (k + 1u - rx_lines() > 4u) {
      if (++patience > 200000000ull) {
        return NULL; // consumer stuck; the counts will say so
      }
    }
  }
  return NULL;
}

TEST("soak: every line parsed in main context, nothing dropped") {
  rx_init();
  pthread_t isr;
  REQUIRE(pthread_create(&isr, NULL, isr_feeder, NULL) == 0);
  uint64_t patience = 0;
  while (rx_lines() < (uint32_t)SOAK_LINES) {
    rx_poll();
    if (++patience > 200000000ull) {
      break; // no progress; the counts below will report
    }
  }
  pthread_join(isr, NULL);
  CHECK_EQ(rx_lines(), (uint32_t)SOAK_LINES);
  CHECK_EQ(rx_dropped(), 0u);
  CHECK_EQ(rx_line_checksum(), (uint32_t)expected_fold(SOAK_LINES));
  CHECK_EQ(rx_work_done_in_isr(), 0u); // ALL parsing happened in main
}
