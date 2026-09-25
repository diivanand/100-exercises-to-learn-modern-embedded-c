// =============================================================================
//  11.01 -- Volatile: the compiler cannot see the hardware
// =============================================================================
//
//  Every optimisation the compiler performs rests on one assumption: memory
//  only changes when the PROGRAM changes it. Hardware breaks that assumption.
//  A UART's status register flips its bits when bytes arrive on a wire the
//  compiler has never heard of, so code like
//
//      while ((uart->status & READY) == 0) { }
//
//  is, to the optimiser, a load that cannot change inside the loop. It is
//  entitled to read `status` ONCE, before the loop, and spin on the stale
//  value forever. At -O0 it happens to re-read every iteration -- which is
//  why this bug ships: debug builds poll fine, the release build hangs on
//  the bench. (NOTE: this exercise is deliberately compiled with -O2 -- see
//  the `optimize` marker file next to it -- so the release build is the one
//  you are debugging right now.)
//
//  `volatile` is the fix, and it is a narrow one (Effective C ch. 2, "Type
//  Qualifiers"). A volatile-qualified access is an OBSERVABLE SIDE EFFECT:
//  the implementation must perform every one, exactly as the abstract
//  machine orders them relative to other volatile accesses. That is the
//  whole promise. In particular volatile does NOT give you:
//
//   - atomicity: a volatile uint64_t still tears on a 32-bit bus (12.01);
//   - ordering of ORDINARY memory around it: the compiler may still move
//     plain stores across a volatile store (11.07);
//   - inter-thread visibility guarantees: threads need <stdatomic.h>, and
//     using volatile instead is the classic pre-C11 bug (12.02).
//
//  Registers are not threads. For a memory-mapped register on one core,
//  volatile is exactly right and atomics are beside the point; between
//  threads it is exactly wrong. Keep the two toolboxes apart.
//
//  THE TEST BELOW stages the hardware with a POSIX thread that sets the
//  ready bit after 50 ms -- a writer outside the compiler's view, which is
//  the property that matters. Run the starter and it either hangs until the
//  harness deadline kills it (the hoisted load), or returns early with the
//  bit still clear (the loop deleted outright -- C17 6.8.5p6 lets the
//  optimiser assume loops like this one terminate). Both are the same
//  disease: the compiler could not see the hardware.
//
//  TASK
//    Make `wait_ready` poll through a volatile access. Qualify the access
//    path -- `const volatile uint32_t *` -- as one does when overlaying
//    registers (compare bsp/l476_regs.h, where the struct members carry the
//    qualifier instead). Do not change the tests.
//
//  RUN IT
//    ./mec test 11_01
//
// =============================================================================

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
  // TODO: nothing in this loop can change `uart->status` -- as far as the
  // optimiser knows. At -O2 the load is hoisted (or the loop discarded).
  while ((uart->status & UART_STATUS_READY) == 0u) {
  }
  return true;
}

// --- the "hardware" ----------------------------------------------------------
// A second thread stands in for the peripheral: something outside the
// compiler's view writes the register while our loop runs. On silicon that
// writer is a flip-flop, and no compiler flag makes it visible.

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
