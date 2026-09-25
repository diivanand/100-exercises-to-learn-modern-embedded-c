// Solution -- 11.01 Volatile: the compiler cannot see the hardware

// POSIX (pthreads, nanosleep) is host-only scaffolding here; glibc hides it
// under a strict -std=c17 unless asked. This must precede every #include.
#define _POSIX_C_SOURCE 200809L

#include <mect/mect.h>

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>

struct fake_uart {
  uint32_t status;
};

#define UART_STATUS_READY (1u << 7)

bool wait_ready(struct fake_uart *uart) {
  // The MMIO idiom: qualify the ACCESS PATH, not necessarily the object.
  // Through this pointer, every read is an observable action the optimiser
  // must perform -- it can no longer prove the loop body never changes the
  // condition and hoist the load out. (bsp/l476_regs.h qualifies the struct
  // members instead, which amounts to the same accesses; qualifying at the
  // pointer is what you do when the struct is not yours to edit.)
  const volatile uint32_t *status = &uart->status;
  while ((*status & UART_STATUS_READY) == 0u) {
  }
  return true;
}

// --- the "hardware" ----------------------------------------------------------
// A second thread stands in for the peripheral: something outside the
// compiler's view writes the register while our loop runs. On silicon that
// writer is a flip-flop, and no compiler flag makes it visible; the thread
// is the closest honest imitation a host test can stage. (Formally this is
// a data race on a plain uint32_t -- chapter 12 introduces the tools that
// make sharing lawful BETWEEN THREADS. A register is not a thread; volatile
// is the right tool here and the wrong one there.)

static void *hardware_thread(void *arg) {
  struct fake_uart *uart = arg;
  const struct timespec delay = {.tv_sec = 0, .tv_nsec = 50 * 1000 * 1000};
  nanosleep(&delay, NULL);
  *(volatile uint32_t *)&uart->status |= UART_STATUS_READY;
  return NULL;
}

TEST("wait_ready sees the bit the hardware sets") {
  struct fake_uart uart = {.status = 0};
  pthread_t hw;
  REQUIRE(pthread_create(&hw, NULL, hardware_thread, &uart) == 0);

  CHECK(wait_ready(&uart));
  CHECK((uart.status & UART_STATUS_READY) != 0u);

  REQUIRE(pthread_join(hw, NULL) == 0);
}
